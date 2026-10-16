/* VRAM image streams, picture and background loading, message blocks, sound novel files, display rects, input repeat,
 * render object visibility, screen fades, proposition rows and availability, the calendar and nine-bit records. */

#include "fft/data.h"
#include "fft/file.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "fft/main_save.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/menu_input.h"
#include "fft/script_variable.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "fft/wldcore.h"
#include "fft/wldcore_gfx.h"
#include "fft/wldcore_map.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_proposition.h"
#include "fft/wldcore_sound.h"
#include "fft/wldcore_sound_novel.h"
#include "fft/wldcore_text.h"
#include "fft/wldcore_window.h"
#include "fft/world_formation.h"
#include "fft/world_menu.h"
#include "fft/world_sprite.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static s32 g_world_map_input_repeat_initial_delay = 0;
static s32 g_world_map_input_repeat_period = 0;
static s32 g_world_map_input_secondary_repeat_period = 0;
static s32 g_world_map_input_repeat_counter_up = 0;
static s32 g_world_map_input_repeat_counter_down = 0;
static s32 g_world_map_input_repeat_counter_left = 0;
static s32 g_world_map_input_repeat_counter_right = 0;
static s32 g_world_map_input_repeat_counter_r1 = 0;
static s32 g_world_map_input_repeat_counter_l1 = 0;

static void calendar_advance_birthdays(void);

/* One step of the CD stream that loads an image file straight into VRAM (the
 * map texture WLDTEX.TM2): set double speed, seek, then read the sectors into
 * the two 2048-byte halves of `destination` in turn. Each sector holds a
 * count of rectangles, each a packed position, a packed size and its pixels,
 * uploaded with LoadImage. A disc error or a stall restarts from the speed
 * change at the current sector.
 *
 * The switch's jump table is the first 32 bytes of WLDCORE.BIN. */
void world_map_poll_texture_stream(file_load_descriptor_t* stream) {
    RECT rect;
    u8 mode[8];
    s32 rect_count;
    s32 word;
    s32 n;
    s32 status;

    switch (stream->state) {
    case FILE_LOAD_STATE_IDLE:
        break;
    case FILE_LOAD_STATE_SET_DOUBLE_SPEED:
        mode[0] = CdlModeSpeed;
        CdControl(CdlSetmode, mode, 0);
        g_system_session_frames = 0;
        stream->state++;
        break;
    case FILE_LOAD_STATE_WAIT_AFTER_SET_MODE:
        if (g_system_session_frames < 4) {
            return;
        }
        stream->state++;
        break;
    case FILE_LOAD_STATE_START_SEEK:
        CdControlF(CdlSetloc, (u8*)&stream->position);
        stream->state++;
        break;
    case FILE_LOAD_STATE_POLL_SEEK:
        status = CdSync(1, 0);
        if (status == CdlComplete) {
            stream->state++;
            break;
        }
        if (status != CdlDiskError) {
            return;
        }
        stream->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
        stream->error_count++;
        return;
    case FILE_LOAD_STATE_START_READ:
        if (CdRead2(CdlModeSpeed) == 0) {
            stream->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            stream->error_count++;
            return;
        }
        stream->wait_frames = 0;
        stream->sector_index = 0;
        stream->state++;
        break;
    case FILE_LOAD_STATE_POLL_READ:
        status = CdReady(1, 0);
        if (status == CdlDataReady) {
            CdGetSector(stream->destination + (stream->sector_index & 1) * 2048, 0x200);
            stream->wait_frames = 0;
            stream->state++;
            break;
        }
        if (status == CdlDiskError) {
            stream->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            stream->error_count++;
        }
        if (stream->wait_frames > 0x40000 || (CdStatus() & 1) != 0) {
            file_reset_cd_subsystems();
            CdIntToPos(stream->lba + stream->sector_index, &stream->position);
            stream->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            stream->error_count++;
            return;
        }
        stream->wait_frames++;
        return;
    case FILE_LOAD_STATE_UPLOAD_VRAM:
        if (CdDataSync(1) != 0) {
            return;
        }
        /* Each sector holds a rectangle count, then per rectangle a packed
         * position, a packed size and the pixels. */
        word = (stream->sector_index & 1) * 512;
        rect_count = ((u32*)stream->destination)[(stream->sector_index & 1) * 512];
        word++;
        for (n = 0; n < rect_count; n++) {
            rect.x = ((u32*)stream->destination)[word];
            rect.y = (((u32*)stream->destination)[word] & 0xFFFF0000) >> 16;
            word++;
            rect.w = ((u32*)stream->destination)[word];
            rect.h = (((u32*)stream->destination)[word] & 0xFFFF0000) >> 16;
            word++;
            LoadImage(&rect, (u32*)stream->destination + word);
            word += rect.w * rect.h / 2;
        }
        stream->sector_index++;
        if (stream->sector_index < stream->sector_count) {
            stream->state = FILE_LOAD_STATE_POLL_READ;
            return;
        }
        stream->state = FILE_LOAD_STATE_IDLE;
        CdControlB(CdlPause, 0, 0);
        return;
    }
}

/* Load the picture data set of location record `index` (stored as set + 1)
 * and return the stored value; 0 means the record has no picture. */
s32 location_load_picture(s32 index) {
    s32 value = g_location_records[index].picture;

    if (value != 0)
        picture_load_set(value - 1);
    return value;
}

/* Load picture set `set` into its image buffer, skipping the transfer when it
 * is already resident. */
void picture_load_set(s32 set) {
    if (set != g_picture_loaded_set) {
        file_wait_for_pending();
        g_picture_loaded_set = set;
        file_wait_then_request_read(&g_file_cd_state, g_picture_sector_offsets[set] + DISC_SECTOR_WORLD_WLDPIC_BIN,
            g_picture_sector_offsets[set + 1] - g_picture_sector_offsets[set], g_picture_buffer);
    }
}

/* Load background set `set` of WLDBK.BIN into the shared scratch buffer,
 * skipping the transfer when it is already resident.
 *
 * Entry i of g_picture_background_sector_offsets is the start sector of set i
 * in the file, so entry i+1 minus entry i is that set's sector count; the
 * other sector-offset tables in this file have the same shape. */
void picture_load_background_set(s32 set) {
    if (set != g_picture_background_loaded_set) {
        file_wait_for_pending();
        g_picture_background_loaded_set = set;
        file_wait_then_request_read(&g_file_cd_state,
            g_picture_background_sector_offsets[set] + DISC_SECTOR_WORLD_WLDBK_BIN,
            g_picture_background_sector_offsets[set + 1] - g_picture_background_sector_offsets[set],
            g_world_map_scratch_buffer);
    }
}

/* Loads message block `block` (message_block_e) unless it is the one already loaded.
 * g_message_block_sector_offsets has the same shape as the pair used by
 * sound_novel_load_files: entry i is the start sector of block i relative to the disc
 * LBA below, so entry i+1 minus entry i is that block's length. */
void message_block_load_if_changed(s32 block) {
    g_text_section_pointers[TEXT_SECTION_MESSAGES] = g_message_block_buffer;
    if (block != g_message_block_loaded) {
        file_wait_for_pending();
        g_message_block_loaded = block;
        file_wait_then_request_read(&g_file_cd_state,
            g_message_block_sector_offsets[block] + DISC_SECTOR_WORLD_WLDMES_BIN,
            g_message_block_sector_offsets[block + 1] - g_message_block_sector_offsets[block], g_message_block_buffer);
    }
}

/* Load sound-novel chapter `chapter`: its script into the save slot buffer
 * and its text into the message buffer, which becomes the message section.
 *
 * Each file comes from a seven-entry sector-offset table. An entry is the
 * start sector of one chapter's data relative to the disc LBA added to it,
 * so entry i+1 minus entry i is that chapter's sector count. */
void sound_novel_load_files(s32 chapter) {
    file_wait_for_pending();
    file_wait_then_request_read(&g_file_cd_state,
        g_sound_novel_script_sector_offsets[chapter] + DISC_SECTOR_WORLD_SNPLBIN_BIN,
        g_sound_novel_script_sector_offsets[chapter + 1] - g_sound_novel_script_sector_offsets[chapter],
        &g_sound_novel_script);
    file_wait_for_pending();
    file_wait_then_request_read(&g_file_cd_state,
        g_sound_novel_text_sector_offsets[chapter] + DISC_SECTOR_WORLD_SNPLMES_BIN,
        g_sound_novel_text_sector_offsets[chapter + 1] - g_sound_novel_text_sector_offsets[chapter],
        g_message_block_buffer);
    g_text_section_pointers[TEXT_SECTION_MESSAGES] = g_message_block_buffer;
    g_message_block_loaded = -1;
}

/* Queue a one-sector read of BATTLE.BIN's first sector with no destination,
 * which seeks the drive there while the event transition plays. */
void event_transition_seek_battle_bin(void) {
    file_wait_then_request_read(&g_file_cd_state, g_file_battle_sector, 1, 0);
}

/* Fill the rectangle with the 256x238 display area of frame
 * buffer 0 (y 0) or 1 (y 0xF0). */
void world_map_set_display_rect(s32 buffer_index, RECT* rect) {
    rect->w = 0x100;
    rect->x = 0;
    rect->h = 0xEE;
    if (buffer_index == 0) {
        rect->y = 0;
    } else {
        rect->y = 0xF0;
    }
}

/* WLDCORE's per-frame pad poll: reads pad 0 and publishes the held buttons
 * and the newly pressed ones (both 0 while SYSTEM_FLAG_INPUT_BLOCKED is
 * set). Exactly L1 + R1 + Select + Start restores the sound volumes and
 * resets the game. */
void world_map_read_pad(void) {
    u32 input;
    u32 previous;

    g_input_previous_buttons = g_input_polled_buttons;
    input = PadRead(0);
    g_input_polled_buttons = input;

    if (g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) {
        g_world_map_input_new_button_presses = 0;
        g_input_current_buttons = 0;
    } else {
        previous = g_input_previous_buttons;
        g_input_current_buttons = input;
        g_world_map_input_new_button_presses = input & ~previous;
    }

    if (g_input_polled_buttons == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        DrawSync(0);
        suzuki_sound_set_master_volume(0x3fff, 1);
        suzuki_sound_put_type(0xc0);
        suzuki_sound_set_cd_volume(0x74ff, 1);
        system_reset_game();
    }
}

/* Step one map scroll axis: direction 1 or -1 accelerates `value` by `step` up to +/-`limit`,
 * direction 0 decays it toward zero by `decay`; `output` is the 8.8 value's whole part, truncated
 * toward zero. */
void world_map_update_scroll_axis(world_map_scroll_axis_t* axis, s32 direction) {
    switch (direction) {
    case 1:
        if (axis->value < axis->limit) {
            axis->value = axis->value + axis->step;
        }
        if (axis->value > axis->limit) {
            axis->value = axis->limit;
        }
        axis->output = axis->value >> 8;
        break;
    case 0:
        if (axis->value == 0) {
            break;
        }
        if (axis->value > 0) {
            axis->value = axis->value - axis->decay;
            if (axis->value <= 0) {
                axis->value = 0;
            }
            axis->output = axis->value >> 8;
        } else {
            axis->value = axis->value + axis->decay;
            if (axis->value >= 0) {
                axis->value = 0;
            }
            axis->output = -((-axis->value) >> 8);
        }
        break;
    case -1:
        if (-axis->value < axis->limit) {
            axis->value = axis->value - axis->step;
        }
        if (-axis->value > axis->limit) {
            axis->value = -axis->limit;
        }
        axis->output = -((-axis->value) >> 8);
        break;
    }
}

/* Clear the six auto-repeat counters and copy the repeat delay and periods from MAIN's input
 * settings. */
void world_map_init_input_repeat(void) {
    g_world_map_input_repeat_counter_l1 = 0;
    g_world_map_input_repeat_counter_r1 = 0;
    g_world_map_input_repeat_counter_right = 0;
    g_world_map_input_repeat_counter_left = 0;
    g_world_map_input_repeat_counter_down = 0;
    g_world_map_input_repeat_counter_up = 0;
    g_world_map_input_repeat_initial_delay = g_input_repeat_initial_delay;
    g_world_map_input_repeat_period = g_input_repeat_period;
    g_world_map_input_secondary_repeat_period = g_input_secondary_repeat_period;
}

/* Return the newly pressed buttons in `buttons`, or else the first held button
 * whose auto-repeat counter fires.
 *
 * The arms test RIGHT before LEFT, unlike the counters' address order, and a
 * new press clears only the four direction counters, not R1 and L1. */
u32 world_map_update_input_repeat(u32 buttons) {
    s32 next;
    u32 pressed = g_world_map_input_new_button_presses;

    pressed &= buttons;
    if (pressed != 0) {
        g_world_map_input_repeat_counter_right = 0;
        g_world_map_input_repeat_counter_left = 0;
        g_world_map_input_repeat_counter_down = 0;
        g_world_map_input_repeat_counter_up = 0;
        return pressed;
    }

    if ((buttons & PSX_PAD_UP) != 0) {
        if ((g_input_current_buttons & PSX_PAD_UP) != 0) {
            s32* counter = &g_world_map_input_repeat_counter_up;

            if (g_world_map_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_world_map_input_repeat_period;
                return PSX_PAD_UP;
            }
        } else {
            g_world_map_input_repeat_counter_up = 0;
        }
    }

    if ((buttons & PSX_PAD_DOWN) != 0) {
        if ((g_input_current_buttons & PSX_PAD_DOWN) != 0) {
            s32* counter = &g_world_map_input_repeat_counter_down;

            if (g_world_map_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_world_map_input_repeat_period;
                return PSX_PAD_DOWN;
            }
        } else {
            g_world_map_input_repeat_counter_down = 0;
        }
    }

    if ((buttons & PSX_PAD_RIGHT) != 0) {
        if ((g_input_current_buttons & PSX_PAD_RIGHT) != 0) {
            s32* counter = &g_world_map_input_repeat_counter_right;

            if (g_world_map_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_world_map_input_repeat_period;
                return PSX_PAD_RIGHT;
            }
        } else {
            g_world_map_input_repeat_counter_right = 0;
        }
    }

    if ((buttons & PSX_PAD_LEFT) != 0) {
        if ((g_input_current_buttons & PSX_PAD_LEFT) != 0) {
            s32* counter = &g_world_map_input_repeat_counter_left;

            if (g_world_map_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_world_map_input_repeat_period;
                return PSX_PAD_LEFT;
            }
        } else {
            g_world_map_input_repeat_counter_left = 0;
        }
    }

    if ((buttons & PSX_PAD_R1) != 0) {
        if ((g_input_current_buttons & PSX_PAD_R1) != 0) {
            s32* counter = &g_world_map_input_repeat_counter_r1;

            if (g_world_map_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_world_map_input_secondary_repeat_period;
                return PSX_PAD_R1;
            }
        } else {
            g_world_map_input_repeat_counter_r1 = 0;
        }
    }

    if ((buttons & PSX_PAD_L1) != 0) {
        if ((g_input_current_buttons & PSX_PAD_L1) != 0) {
            s32* counter = &g_world_map_input_repeat_counter_l1;
            s32 old_counter;
            s32 last_next;

            /* Spelled apart from the other arms: separate old and next values
             * give this last arm the retail register assignment. */
            old_counter = *counter;
            if (g_world_map_input_repeat_initial_delay < (*counter = last_next = old_counter + 1)) {
                old_counter = last_next - g_world_map_input_secondary_repeat_period;
                *counter = old_counter;
                return PSX_PAD_L1;
            }
        } else {
            g_world_map_input_repeat_counter_l1 = 0;
        }
    }

    return 0;
}

/* Each queued record starts with the shared 32-bit display flags. */
static inline s32* wldcore_display_object_flags(void* object) {
    return object;
}

/* Clear (nonzero argument) or set the blank flag on both display-object queues. */
void window_set_render_objects_visible(s32 clear) {
    s32 i;

    if (clear) {
        for (i = 0; i < g_window_aux_render_object_count; i++) {
            *wldcore_display_object_flags(g_window_aux_render_object_queue[i]) &= ~DISPLAY_OBJECT_BLANK;
        }
        for (i = 0; i < g_window_render_object_count; i++) {
            *wldcore_display_object_flags(g_window_render_object_queue[i]) &= ~DISPLAY_OBJECT_BLANK;
        }
    } else {
        for (i = 0; i < g_window_aux_render_object_count; i++) {
            *wldcore_display_object_flags(g_window_aux_render_object_queue[i]) |= DISPLAY_OBJECT_BLANK;
        }
        for (i = 0; i < g_window_render_object_count; i++) {
            *wldcore_display_object_flags(g_window_render_object_queue[i]) |= DISPLAY_OBJECT_BLANK;
        }
    }
}

/* Start a screen fade of `duration` frames unless the current fade already has the same direction
 * (WORLD_MAP_FADE_OUT bit); START_MUSIC also queues the world map theme with a fade-in,
 * STOP_MUSIC fades the music out and unloads it. */
void world_map_fade_start(s32 flags, s32 duration) {
    if ((g_world_map_fade.flags[0] & WORLD_MAP_FADE_OUT) != (flags & WORLD_MAP_FADE_OUT)) {
        g_world_map_fade.flags[0] = flags | WORLD_MAP_FADE_RUNNING;
        g_world_map_fade.elapsed = 0;
        g_world_map_fade.duration = duration;
        g_system_flags |= SYSTEM_FLAG_FADING;
        if (flags & WORLD_MAP_FADE_START_MUSIC) {
            audio_queue_add(AUDIO_COMMAND_PLAY_MUSIC, AUDIO_MUSIC(1, MUSIC_WORLD_MAP));
            audio_queue_add(AUDIO_COMMAND_FADE_IN, 0x10);
        }
        if (flags & WORLD_MAP_FADE_STOP_MUSIC) {
            audio_queue_add(AUDIO_COMMAND_FADE_OUT, 4);
            audio_queue_add(AUDIO_COMMAND_UNLOAD, 2);
        }
    }
}

/* Steps the world screen fade: ramps the menu brightness and the first fill box
 * color toward the target until the elapsed counter reaches the duration. */
s32 world_map_fade_step(void) {
    u32 flags;
    s32 elapsed;
    s32 brightness;
    s32 box_level;

    flags = g_world_map_fade.flags[0];
    if (flags & WORLD_MAP_FADE_RUNNING) {
        if (g_world_map_fade.elapsed == g_world_map_fade.duration) {
            g_world_map_fade.flags[0]
                = flags & ~(WORLD_MAP_FADE_RUNNING | WORLD_MAP_FADE_REDRAW_MAP | WORLD_MAP_FADE_BOX_VISIBLE);
        } else {
            if (g_world_map_fade.elapsed + 1 == g_world_map_fade.duration) {
                g_system_flags &= ~SYSTEM_FLAG_FADING;
                audio_queue_wait_until_empty();
            }
            if (g_world_map_fade.flags[0] & WORLD_MAP_FADE_OUT) {
                elapsed = g_world_map_fade.elapsed;
                brightness = 0x70 - (elapsed << 7) / g_world_map_fade.duration;
                box_level = (elapsed << 8) / g_world_map_fade.duration + 0x20;
                if (brightness < 0) {
                    brightness = 0;
                }
                if (box_level >= 0x100) {
                    box_level = 0xFF;
                }
                if (elapsed < 0) {
                    brightness = 0x80;
                    box_level = 0;
                }
            } else {
                elapsed = g_world_map_fade.elapsed;
                brightness = (elapsed << 7) / g_world_map_fade.duration + 0x10;
                box_level = 0xC0 - (elapsed << 8) / g_world_map_fade.duration;
                if (brightness > 0x80) {
                    brightness = 0x80;
                }
                if (box_level < 0) {
                    box_level = 0;
                }
                if (elapsed < 0) {
                    brightness = 0;
                    box_level = 0x80;
                }
            }
            g_world_map_fade.elapsed++;
            screen_set_brightness(brightness, brightness, brightness);
            g_world_map_brightness_rgb.r = brightness;
            g_world_map_brightness_rgb.g = brightness;
            g_world_map_brightness_rgb.b = brightness;
            g_world_map_fade.boxes[0].r = box_level;
            g_world_map_fade.boxes[0].g = box_level;
            g_world_map_fade.boxes[0].b = box_level;
            if (g_world_map_fade.flags[0] & WORLD_MAP_FADE_REDRAW_MAP) {
                g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
            }
            return 1;
        }
    }

    return 0;
}

/* Copies a packed 24-bit RGB triple, applying the global per-channel tint
 * when bit 0 of the 32-bit fade state is set. Each channel is scaled by
 * ((c << 16) / 128) * factor >> 16, i.e. c * factor / 128, and clamped to
 * 255 with no lower clamp; with the factors at their 0x80 default the scale
 * is the identity. When the bit is clear the triple is copied verbatim,
 * which the 4-byte alignment-1 struct assignment emits as the unaligned
 * lwl/lwr + swl/swr quartet. */
void world_map_fade_tint_color(CVECTOR* source, CVECTOR* out) {
    s32 red;
    s32 green;
    s32 blue;

    if (g_world_map_fade.flags[0] & WORLD_MAP_FADE_RUNNING) {
        red = ((source->r << 16) / 128 * g_world_map_brightness_rgb.r) >> 16;
        green = ((source->g << 16) / 128 * g_world_map_brightness_rgb.g) >> 16;
        blue = ((source->b << 16) / 128 * g_world_map_brightness_rgb.b) >> 16;
        if (red >= 256) {
            red = 255;
        }
        if (green >= 256) {
            green = 255;
        }
        if (blue >= 256) {
            blue = 255;
        }
        out->r = red;
        out->g = green;
        out->b = blue;
    } else {
        *out = *source;
    }
}

/* Sort the two screen-fade boxes into the active ordering table at their
 * priorities. A box is drawn while its fade runs or it is flagged visible,
 * and only when its color is not black. */
void world_map_fade_draw_boxes(void) {
    u32* flags;
    GsBOXF* boxes;
    s32* priorities;
    s32 i;

    flags = g_world_map_fade.flags;
    boxes = g_world_map_fade.boxes;
    for (i = 0; i < 2; i++) {
        /* Reloaded inside the loop so that the priority walk, not its invariant
         * copy, holds the register the exit test compares. */
        priorities = g_world_map_fade.priorities;
        if (flags[i] & (WORLD_MAP_FADE_RUNNING | WORLD_MAP_FADE_BOX_VISIBLE)) {
            if (g_world_map_fade.boxes[i].r != 0 || g_world_map_fade.boxes[i].g != 0
                || g_world_map_fade.boxes[i].b != 0) {
                GsSortBoxFill(&boxes[i], &g_world_map_ordering_tables[g_gfx_active_buffer_index], priorities[i]);
            }
        }
    }
}

/* Return the address of proposition data record `record`: the data block starts with a table of
 * byte offsets from its own start. */
void* proposition_get_data(proposition_data_record_e record) {
    return (u8*)g_proposition_data + g_proposition_data[record];
}

/* Expand one 23-byte proposition row into 16-bit fields, then remap the
 * two fee fields through the 16-bit table of record 1. */
void proposition_unpack_row(proposition_fields_t* out, s32 index) {
    u8* rows = proposition_get_data(PROPOSITION_DATA_ROWS);
    u16* field_lookup;
    s32 i;
    u16* fields = &out->category_index;

    for (i = 0; i < 23; i++) {
        fields[i] = rows[index * 23 + i];
    }
    field_lookup = proposition_get_data(PROPOSITION_DATA_FEES);
    out->fee_a = field_lookup[out->fee_a];
    out->fee_b = field_lookup[out->fee_b];
}

/* Whether proposition `index` is offered at `location`: the row's location
 * (0 for any), then each condition its flags select: a shop stock level, the
 * current zodiac sign, or another proposition (condition_value) completed. */
s32 proposition_is_available_at(s32 index, s32 location) {
    proposition_fields_t entry;
    s32 month;
    s32 day;

    proposition_unpack_row(&entry, index);
    if (entry.location != 0) {
        if (location != entry.location) {
            return 0;
        }
    }
    if (entry.condition_flags & PROPOSITION_CONDITION_SHOP_AVAILABILITY) {
        if (script_get_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) < entry.required_shop_availability) {
            return 0;
        }
    }
    if (entry.condition_flags & PROPOSITION_CONDITION_ZODIAC_SIGN) {
        month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
        day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
        calendar_convert_date_to_zodiac(&month, &day);
        if (month != entry.condition_value) {
            return 0;
        }
    }
    if (entry.condition_flags & PROPOSITION_CONDITION_STATUS) {
        if ((script_get_variable(entry.condition_value + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
                & PROPOSITION_VARIABLE_COMPLETED)
            == 0) {
            return 0;
        }
    }
    return 1;
}

/*
 * Advance the calendar by 1 day and run the daily updates.
 *
 * EVENT_SCRIPT_VAR_UNKNOWN_67 increases on 21 March (the start of Aries) and
 * is capped at 99; calendar_advance_days raises it at the year's end
 * instead, without the cap. The target does not validate the incoming month
 * or day.
 */
void calendar_advance_day(void) {
    s32 month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    s32 day = script_get_variable(EVENT_SCRIPT_VAR_DAY) + 1;

    if (g_unit_birthday_month_lengths[month - 1] < day) {
        month++;
        if (month == 13) {
            month = 1;
        }
        day = 1;
    }
    script_set_variable(EVENT_SCRIPT_VAR_MONTH, month);
    script_set_variable(EVENT_SCRIPT_VAR_DAY, day);
    if (month == 3 && day == 21) {
        s32 value = script_get_variable(EVENT_SCRIPT_VAR_UNKNOWN_67) + 1;
        if (value >= 100) {
            value = 99;
        }
        script_set_variable(EVENT_SCRIPT_VAR_UNKNOWN_67, value);
    }
    proposition_advance_day();
    calendar_advance_birthdays();
    breeding_update(1);
}

/* Converts a calendar date in place into a zodiac sign number (1 = Aries) and
 * the day within that sign. An invalid date plays SOUND_SFX_INVALID and
 * becomes 1/1. */
void calendar_convert_date_to_zodiac(s32* month_ptr, s32* day_ptr) {
    s32 index = 0;
    s32 delta;
    s32 previous_month;
    calendar_date_t* start_dates;
    u8* start_days;
    calendar_date_t* previous_dates;

    if (*month_ptr < 1 || *month_ptr > 12 || *day_ptr < 1 || *day_ptr > 31) {
        sound_play_effect(SOUND_SFX_INVALID);
        *day_ptr = 1;
        *month_ptr = 1;
        return;
    }
    start_dates = g_calendar_zodiac_start_dates;
    while (start_dates[index].month != *month_ptr) {
        index = (index + 1) % 12;
    }
    /* Direct .day indexing folds the two original indexed addresses into one. */
    start_days = &g_calendar_zodiac_start_dates[0].day;
    if (*day_ptr < start_days[index * sizeof(calendar_date_t)]) {
        index = (index + 11) % 12;
        *month_ptr = index + 1;
        /* Separate month-table local preserves the original register allocation. */
        previous_dates = g_calendar_zodiac_start_dates;
        previous_month = previous_dates[index].month;
        delta = start_days[index * sizeof(calendar_date_t)];
        delta = g_unit_birthday_month_lengths[previous_month - 1] - delta + 1;
        *day_ptr += delta;
    } else {
        *month_ptr = index + 1;
        *day_ptr = *day_ptr - start_days[index * sizeof(calendar_date_t)] + 1;
    }
}

/*
 * The daily birthday sweep, run by calendar_advance_day after the
 * calendar rolls over: every brave-story character whose stored birthday is
 * today gets a year older, capped at 99.
 *
 * Entry 0 is the player character. Its row in the birthday table is a
 * placeholder (1/1); the real birthday lives in
 * EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH and _DAY, written when the player picks
 * one at game start, so entry 0 is compared against those two variables and
 * the remaining 63 entries against the table. The table is a 64 x 2 array of
 * month/day byte pairs (g_brave_story_birthday_table) reached through
 * the pointer g_brave_story_birthdays.
 *
 * The age column is walked one byte at a time from index 1, which is why the
 * loop's base materializes as g_brave_story_character_ages + 1; entry 0
 * is the only absolute access to the array.
 */
static void calendar_advance_birthdays(void) {
    const u8(*birthdays)[2] = g_brave_story_birthdays;
    s32 day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
    s32 month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    s32 birth_month = script_get_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH);
    s32 birth_day = script_get_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY);
    u8 age;
    s32 i;

    if (month == birth_month && day == birth_day) {
        age = g_brave_story_character_ages[0];
        if (age < 99) {
            g_brave_story_character_ages[0] = age + 1;
        }
    }

    for (i = 1; i < 64; i++) {
        if (month == birthdays[i][0] && day == birthdays[i][1]) {
            if (g_brave_story_character_ages[i] < 99) {
                g_brave_story_character_ages[i]++;
            }
        }
    }
}

/* WLDCORE's wrapper for party_get_unit: the roster record of slot `party_index`. */
party_unit_t* party_get_roster_unit(s32 party_index) {
    return party_get_unit(party_index);
}

/* Writes one nine-bit record: bits 0-4 from source->day, bits 5-8 from
 * source->month. Records are packed nine bits apart in the bit array. */
void calendar_write_date(u32* bits, s32 record, calendar_date_t* source) {
    s32 base = record * 9;
    s32 mask;
    s32 i;

    mask = 1;
    for (i = 0; i < 5; i++) {
        bits_set_value(bits, base + i, source->day & mask);
        mask <<= 1;
    }
    mask = 1;
    for (i = 0; i < 4; i++) {
        /* The separate temporary keeps the target's add order: folding
         * base + (i + 5) in one expression hoists base + 5 out of the loop. */
        s32 index = i + 5;

        bits_set_value(bits, base + index, source->month & mask);
        mask <<= 1;
    }
}

/* Reads one nine-bit record: bits 0-4 into dest->day, bits 5-8 into dest->month.
 * Records are packed nine bits apart in the bit array, matching
 * calendar_write_date. A field that decodes to zero or beyond its
 * range falls back to 1. */
void calendar_read_date(u32* bits, s32 record, calendar_date_t* dest) {
    s32 base = record * 9;
    s32 mask;
    s32 i;

    mask = 1;
    dest->day = 0;
    for (i = 0; i < 5; i++) {
        if (bits_test(bits, base + i)) {
            dest->day |= mask;
        }
        mask <<= 1;
    }
    mask = 1;
    dest->month = 0;
    for (i = 0; i < 4; i++) {
        s32 index = i + 5;

        if (bits_test(bits, base + index)) {
            dest->month |= mask;
        }
        mask <<= 1;
    }
    if (dest->day == 0 || dest->day >= 32) {
        dest->day = 1;
    }
    if (dest->month == 0 || dest->month >= 13) {
        dest->month = 1;
    }
}

/* Clear both frame buffers (256 x 480 at the VRAM origin) to black and, when
 * asked, blank the 256x240 backdrop image in g_world_map_scratch_buffer. */
void world_map_clear_vram_and_scratch(s32 clear_scratch) {
    RECT image_rect;
    s32 i;

    DrawSync(0);
    VSync(0);
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 0x100;
    image_rect.h = 0x1e0;
    ClearImage(&image_rect, 0, 0, 0);
    if (clear_scratch != 0) {
        for (i = WLDCORE_BACKDROP_WORDS - 1; i >= 0; i--) {
            g_world_map_scratch_buffer[i] = 0;
        }
    }
    DrawSync(0);
}
