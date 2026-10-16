/* VRAM image streams, picture and background loading, message blocks, sound novel files, display rects, input repeat,
 * render object visibility, screen fades, proposition rows and availability, the calendar and nine-bit records. */

#include "fft/data.h"
#include "fft/file.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/unit.h"
#include "fft/wldcore.h"
#include "fft/wldcore_map.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_proposition.h"
#include "fft/wldcore_script.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "fft/world_menu.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static void wldcore_advance_brave_story_birthdays(void);

/* Streams CD-resident image chunks to VRAM through a retrying state machine.
 *
 * The switch jump table occupies 0x80067000-0x80067020, at the start of
 * WLDCORE.BIN. */
void wldcore_file_poll_vram_image_stream(main_file_load_descriptor_t* stream) {
    RECT rect;
    u8 mode[8];
    s32 rect_count;
    s32 word;
    s32 n;
    s32 status;

    switch (stream->state) {
    case MAIN_FILE_LOAD_STATE_IDLE:
        break;
    case MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED:
        mode[0] = CdlModeSpeed;
        CdControl(CdlSetmode, mode, 0);
        g_main_system_session_frames = 0;
        stream->state++;
        break;
    case MAIN_FILE_LOAD_STATE_WAIT_AFTER_SET_MODE:
        if (g_main_system_session_frames < 4) {
            return;
        }
        stream->state++;
        break;
    case MAIN_FILE_LOAD_STATE_START_SEEK:
        CdControlF(CdlSetloc, (u8*)&stream->position);
        stream->state++;
        break;
    case MAIN_FILE_LOAD_STATE_POLL_SEEK:
        status = CdSync(1, 0);
        if (status == CdlComplete) {
            stream->state++;
            break;
        }
        if (status != CdlDiskError) {
            return;
        }
        stream->state = MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED;
        stream->error_count++;
        return;
    case MAIN_FILE_LOAD_STATE_START_READ:
        if (CdRead2(CdlModeSpeed) == 0) {
            stream->state = MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            stream->error_count++;
            return;
        }
        stream->wait_frames = 0;
        stream->sector_index = 0;
        stream->state++;
        break;
    case MAIN_FILE_LOAD_STATE_POLL_READ:
        status = CdReady(1, 0);
        if (status == CdlDataReady) {
            CdGetSector(stream->destination + (stream->sector_index & 1) * 2048, 0x200);
            stream->wait_frames = 0;
            stream->state++;
            break;
        }
        if (status == CdlDiskError) {
            stream->state = MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            stream->error_count++;
        }
        if (stream->wait_frames > 0x40000 || (CdStatus() & 1) != 0) {
            main_file_reset_cd_subsystems();
            CdIntToPos(stream->lba + stream->sector_index, &stream->position);
            stream->state = MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            stream->error_count++;
            return;
        }
        stream->wait_frames++;
        return;
    case MAIN_FILE_LOAD_STATE_UPLOAD_VRAM:
        if (CdDataSync(1) != 0) {
            return;
        }
        /* Each sector has a count, then pairs of packed 32-bit XY/WH values and 32-bit image data. */
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
            stream->state = MAIN_FILE_LOAD_STATE_POLL_READ;
            return;
        }
        stream->state = MAIN_FILE_LOAD_STATE_IDLE;
        CdControlB(CdlPause, 0, 0);
        return;
    }
}

/* Load the picture data set of location record `index` (stored as set + 1)
 * and return the stored value; 0 means the record has no picture. */
s32 wldcore_proposition_load_picture(s32 index) {
    s32 value = g_wldcore_location_records[index].picture;

    if (value != 0)
        wldcore_proposition_load_picture_data_set(value - 1);
    return value;
}

/* Load picture set `set` into its image buffer, skipping the transfer when it
 * is already resident. */
void wldcore_proposition_load_picture_data_set(s32 set) {
    if (set != g_wldcore_loaded_picture_set) {
        file_wait_for_pending_variant();
        g_wldcore_loaded_picture_set = set;
        file_wait_then_build_header(&g_main_file_cd_state,
            g_wldcore_picture_sector_offsets[set] + DISC_SECTOR_WORLD_WLDPIC_BIN,
            g_wldcore_picture_sector_offsets[set + 1] - g_wldcore_picture_sector_offsets[set],
            g_wldcore_picture_buffer);
    }
}

/* g_wldcore_background_sector_offsets is a sector-offset table whose entry i is the start sector of set i
 * relative to the disc LBA below it, so entry i+1 minus entry i is that set's
 * sector count. Same shape as g_wldcore_sound_novel_script_sector_offsets and
 * g_wldcore_sound_novel_text_sector_offsets used by
 * wldcore_load_sound_novel_files. */

/* Load set `set` into the shared scratch buffer, skipping the transfer when it
 * is already resident. */
void wldcore_gfx_load_background_set(s32 set) {
    if (set != g_wldcore_loaded_background_set) {
        file_wait_for_pending_variant();
        g_wldcore_loaded_background_set = set;
        file_wait_then_build_header(&g_main_file_cd_state,
            g_wldcore_background_sector_offsets[set] + DISC_SECTOR_WORLD_WLDBK_BIN,
            g_wldcore_background_sector_offsets[set + 1] - g_wldcore_background_sector_offsets[set],
            g_wldcore_scratch_buffer);
    }
}

/* Loads message block `block` (wldcore_message_block_e) unless it is the one already loaded.
 * g_wldcore_message_block_sector_offsets has the same shape as the pair used by
 * wldcore_load_sound_novel_files: entry i is the start sector of block i relative to the disc
 * LBA below, so entry i+1 minus entry i is that block's length. */
void wldcore_load_message_block_if_changed(s32 block) {
    g_text_section_pointers[TEXT_SECTION_MESSAGES] = g_wldcore_message_buffer;
    if (block != g_wldcore_loaded_message_block) {
        file_wait_for_pending_variant();
        g_wldcore_loaded_message_block = block;
        file_wait_then_build_header(&g_main_file_cd_state,
            g_wldcore_message_block_sector_offsets[block] + DISC_SECTOR_WORLD_WLDMES_BIN,
            g_wldcore_message_block_sector_offsets[block + 1] - g_wldcore_message_block_sector_offsets[block],
            g_wldcore_message_buffer);
    }
}

/* Load sound-novel chapter `chapter`: its script into the save slot buffer
 * and its text into the message buffer, which becomes the message section.
 *
 * Each file comes from a seven-entry sector-offset table. An entry is the
 * start sector of one chapter's data relative to the disc LBA added to it,
 * so entry i+1 minus entry i is that chapter's sector count. */
void wldcore_load_sound_novel_files(s32 chapter) {
    file_wait_for_pending_variant();
    file_wait_then_build_header(&g_main_file_cd_state,
        g_wldcore_sound_novel_script_sector_offsets[chapter] + DISC_SECTOR_WORLD_SNPLBIN_BIN,
        g_wldcore_sound_novel_script_sector_offsets[chapter + 1] - g_wldcore_sound_novel_script_sector_offsets[chapter],
        &g_main_save_slot_buffer);
    file_wait_for_pending_variant();
    file_wait_then_build_header(&g_main_file_cd_state,
        g_wldcore_sound_novel_text_sector_offsets[chapter] + DISC_SECTOR_WORLD_SNPLMES_BIN,
        g_wldcore_sound_novel_text_sector_offsets[chapter + 1] - g_wldcore_sound_novel_text_sector_offsets[chapter],
        g_wldcore_message_buffer);
    g_text_section_pointers[TEXT_SECTION_MESSAGES] = g_wldcore_message_buffer;
    g_wldcore_loaded_message_block = -1;
}

/* Queue a one-sector load at BATTLE.BIN's sector with a null destination.
 * Called as the event transition level is pushed. */
/* Target 0x80068d40. */
void wldcore_file_seek_to_battle_bin(void) {
    file_wait_then_build_header(&g_main_file_cd_state, g_main_battle_bin_sector, 1, 0);
}

/* Fill the rectangle with the 256x238 display area of frame
 * buffer 0 (y 0) or 1 (y 0xF0). */
void wldcore_gfx_set_display_rect(s32 buffer_index, RECT* rect) {
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
 * and the newly pressed ones (both 0 while MAIN_SYSTEM_FLAG_INPUT_BLOCKED is
 * set). Exactly L1 + R1 + Select + Start restores the sound volumes and
 * resets the game. */
void wldcore_reset_game_if_special_keycode_is_pressed(void) {
    u32 input;
    u32 previous;

    g_input_previous_buttons = g_input_polled_buttons;
    input = PadRead(0);
    g_input_polled_buttons = input;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) {
        g_wldcore_input_new_button_presses = 0;
        g_input_current_buttons = 0;
    } else {
        previous = g_input_previous_buttons;
        g_input_current_buttons = input;
        g_wldcore_input_new_button_presses = input & ~previous;
    }

    if (g_input_polled_buttons == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        DrawSync(0);
        suzuki_sound_set_master_volume(0x3fff, 1);
        suzuki_sound_put_type(0xc0);
        suzuki_sound_set_cd_volume(0x74ff, 1);
        main_system_reset_game();
    }
}

/* Step one map scroll axis: direction 1 or -1 accelerates `value` by `step` up to +/-`limit`,
 * direction 0 decays it toward zero by `decay`; `output` is the 8.8 value's whole part, truncated
 * toward zero. */
void wldcore_map_update_scroll_axis_speed(wldcore_axis_state_t* axis, s32 direction) {
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
void wldcore_input_init_repeat_state(void) {
    g_wldcore_input_repeat_counter_l1 = 0;
    g_wldcore_input_repeat_counter_r1 = 0;
    g_wldcore_input_repeat_counter_right = 0;
    g_wldcore_input_repeat_counter_left = 0;
    g_wldcore_input_repeat_counter_down = 0;
    g_wldcore_input_repeat_counter_up = 0;
    g_wldcore_input_repeat_initial_delay = g_main_input_repeat_initial_delay;
    g_wldcore_input_repeat_period = g_main_input_repeat_period;
    g_wldcore_input_secondary_repeat_period = g_main_input_secondary_repeat_period;
}

/* Return the newly pressed buttons in `buttons`, or else the first held button
 * whose auto-repeat counter fires.
 *
 * The arms test RIGHT before LEFT, unlike the counters' address order, and a
 * new press clears only the four direction counters, not R1 and L1. */
u32 wldcore_input_update_repeat(u32 buttons) {
    s32 next;
    u32 pressed = g_wldcore_input_new_button_presses;

    pressed &= buttons;
    if (pressed != 0) {
        g_wldcore_input_repeat_counter_right = 0;
        g_wldcore_input_repeat_counter_left = 0;
        g_wldcore_input_repeat_counter_down = 0;
        g_wldcore_input_repeat_counter_up = 0;
        return pressed;
    }

    if ((buttons & PSX_PAD_UP) != 0) {
        if ((g_input_current_buttons & PSX_PAD_UP) != 0) {
            s32* counter = &g_wldcore_input_repeat_counter_up;

            if (g_wldcore_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_wldcore_input_repeat_period;
                return PSX_PAD_UP;
            }
        } else {
            g_wldcore_input_repeat_counter_up = 0;
        }
    }

    if ((buttons & PSX_PAD_DOWN) != 0) {
        if ((g_input_current_buttons & PSX_PAD_DOWN) != 0) {
            s32* counter = &g_wldcore_input_repeat_counter_down;

            if (g_wldcore_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_wldcore_input_repeat_period;
                return PSX_PAD_DOWN;
            }
        } else {
            g_wldcore_input_repeat_counter_down = 0;
        }
    }

    if ((buttons & PSX_PAD_RIGHT) != 0) {
        if ((g_input_current_buttons & PSX_PAD_RIGHT) != 0) {
            s32* counter = &g_wldcore_input_repeat_counter_right;

            if (g_wldcore_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_wldcore_input_repeat_period;
                return PSX_PAD_RIGHT;
            }
        } else {
            g_wldcore_input_repeat_counter_right = 0;
        }
    }

    if ((buttons & PSX_PAD_LEFT) != 0) {
        if ((g_input_current_buttons & PSX_PAD_LEFT) != 0) {
            s32* counter = &g_wldcore_input_repeat_counter_left;

            if (g_wldcore_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_wldcore_input_repeat_period;
                return PSX_PAD_LEFT;
            }
        } else {
            g_wldcore_input_repeat_counter_left = 0;
        }
    }

    if ((buttons & PSX_PAD_R1) != 0) {
        if ((g_input_current_buttons & PSX_PAD_R1) != 0) {
            s32* counter = &g_wldcore_input_repeat_counter_r1;

            if (g_wldcore_input_repeat_initial_delay < (*counter = next = *counter + 1)) {
                *counter = next - g_wldcore_input_secondary_repeat_period;
                return PSX_PAD_R1;
            }
        } else {
            g_wldcore_input_repeat_counter_r1 = 0;
        }
    }

    if ((buttons & PSX_PAD_L1) != 0) {
        if ((g_input_current_buttons & PSX_PAD_L1) != 0) {
            s32* counter = &g_wldcore_input_repeat_counter_l1;
            s32 old_counter;
            s32 last_next;

            old_counter = *counter;
            if (g_wldcore_input_repeat_initial_delay < (*counter = last_next = old_counter + 1)) {
                old_counter = last_next - g_wldcore_input_secondary_repeat_period;
                *counter = old_counter;
                return PSX_PAD_L1;
            }
        } else {
            g_wldcore_input_repeat_counter_l1 = 0;
        }
    }

    return 0;
}

/* Each queued record starts with the shared 32-bit display flags. */
static inline s32* wldcore_display_object_flags(void* object) {
    return object;
}

/* Clear (nonzero argument) or set the blank flag on both display-object queues. */
void wldcore_window_set_render_objects_visible(s32 clear) {
    s32 i;

    if (clear) {
        for (i = 0; i < g_wldcore_window_aux_render_object_count; i++) {
            *wldcore_display_object_flags(g_wldcore_window_aux_render_object_queue[i]) &= ~WORLD_RENDER_RECORD_BLANK;
        }
        for (i = 0; i < g_wldcore_window_render_object_count; i++) {
            *wldcore_display_object_flags(g_wldcore_window_render_object_queue[i]) &= ~WORLD_RENDER_RECORD_BLANK;
        }
    } else {
        for (i = 0; i < g_wldcore_window_aux_render_object_count; i++) {
            *wldcore_display_object_flags(g_wldcore_window_aux_render_object_queue[i]) |= WORLD_RENDER_RECORD_BLANK;
        }
        for (i = 0; i < g_wldcore_window_render_object_count; i++) {
            *wldcore_display_object_flags(g_wldcore_window_render_object_queue[i]) |= WORLD_RENDER_RECORD_BLANK;
        }
    }
}

/* Start a screen fade of `duration` frames unless the current fade already has the same direction
 * (WLDCORE_SCREEN_FADE_OUT bit); START_MUSIC also queues the world map theme with a fade-in,
 * STOP_MUSIC fades the music out and unloads it. */
void wldcore_fade_start_screen(s32 flags, s32 duration) {
    if ((g_wldcore_screen_fade_state.flags[0] & WLDCORE_SCREEN_FADE_OUT) != (flags & WLDCORE_SCREEN_FADE_OUT)) {
        g_wldcore_screen_fade_state.flags[0] = flags | WLDCORE_SCREEN_FADE_RUNNING;
        g_wldcore_screen_fade_state.elapsed = 0;
        g_wldcore_screen_fade_state.duration = duration;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_FADING;
        if (flags & WLDCORE_SCREEN_FADE_START_MUSIC) {
            wldcore_sound_enqueue_audio_command(
                WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 0x10);
        }
        if (flags & WLDCORE_SCREEN_FADE_STOP_MUSIC) {
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 4);
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_UNLOAD, 2);
        }
    }
}

/* Steps the world screen fade: ramps the menu brightness and the first fill box
 * color toward the target until the elapsed counter reaches the duration. */
s32 wldcore_fade_step_screen(void) {
    u32 flags;
    s32 elapsed;
    s32 brightness;
    s32 box_level;

    flags = g_wldcore_screen_fade_state.flags[0];
    if (flags & WLDCORE_SCREEN_FADE_RUNNING) {
        if (g_wldcore_screen_fade_state.elapsed == g_wldcore_screen_fade_state.duration) {
            g_wldcore_screen_fade_state.flags[0] = flags
                & ~(WLDCORE_SCREEN_FADE_RUNNING | WLDCORE_SCREEN_FADE_REDRAW_MAP | WLDCORE_SCREEN_FADE_BOX_VISIBLE);
        } else {
            if (g_wldcore_screen_fade_state.elapsed + 1 == g_wldcore_screen_fade_state.duration) {
                g_main_system_flags &= ~MAIN_SYSTEM_FLAG_FADING;
                wldcore_sound_wait_for_queue_drain();
            }
            if (g_wldcore_screen_fade_state.flags[0] & WLDCORE_SCREEN_FADE_OUT) {
                elapsed = g_wldcore_screen_fade_state.elapsed;
                brightness = 0x70 - (elapsed << 7) / g_wldcore_screen_fade_state.duration;
                box_level = (elapsed << 8) / g_wldcore_screen_fade_state.duration + 0x20;
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
                elapsed = g_wldcore_screen_fade_state.elapsed;
                brightness = (elapsed << 7) / g_wldcore_screen_fade_state.duration + 0x10;
                box_level = 0xC0 - (elapsed << 8) / g_wldcore_screen_fade_state.duration;
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
            g_wldcore_screen_fade_state.elapsed++;
            world_menu_set_brightness(brightness, brightness, brightness);
            g_wldcore_screen_brightness_rgb.r = brightness;
            g_wldcore_screen_brightness_rgb.g = brightness;
            g_wldcore_screen_brightness_rgb.b = brightness;
            g_wldcore_screen_fade_state.boxes[0].r = box_level;
            g_wldcore_screen_fade_state.boxes[0].g = box_level;
            g_wldcore_screen_fade_state.boxes[0].b = box_level;
            if (g_wldcore_screen_fade_state.flags[0] & WLDCORE_SCREEN_FADE_REDRAW_MAP) {
                g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
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
void wldcore_gfx_copy_color_with_tint(CVECTOR* source, CVECTOR* out) {
    s32 red;
    s32 green;
    s32 blue;

    if (g_wldcore_screen_fade_state.flags[0] & WLDCORE_SCREEN_FADE_RUNNING) {
        red = ((source->r << 16) / 128 * g_wldcore_screen_brightness_rgb.r) >> 16;
        green = ((source->g << 16) / 128 * g_wldcore_screen_brightness_rgb.g) >> 16;
        blue = ((source->b << 16) / 128 * g_wldcore_screen_brightness_rgb.b) >> 16;
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
void wldcore_fade_draw_screen_overlays(void) {
    u32* flags;
    GsBOXF* boxes;
    s32* priorities;
    s32 i;

    flags = g_wldcore_screen_fade_state.flags;
    boxes = g_wldcore_screen_fade_state.boxes;
    for (i = 0; i < 2; i++) {
        /* Reloaded inside the loop so that the priority walk, not its invariant
         * copy, holds the register the exit test compares. */
        priorities = g_wldcore_screen_fade_state.priorities;
        if (flags[i] & (WLDCORE_SCREEN_FADE_RUNNING | WLDCORE_SCREEN_FADE_BOX_VISIBLE)) {
            if (g_wldcore_screen_fade_state.boxes[i].r != 0 || g_wldcore_screen_fade_state.boxes[i].g != 0
                || g_wldcore_screen_fade_state.boxes[i].b != 0) {
                GsSortBoxFill(&boxes[i], &g_wldcore_gfx_ordering_tables[g_active_graphics_buffer_index], priorities[i]);
            }
        }
    }
}

/* Return the address of proposition data record `record`: the data block starts with a table of
 * byte offsets from its own start. */
void* wldcore_proposition_get_data_pointer(wldcore_proposition_data_record_e record) {
    return (u8*)g_wldcore_proposition_data_base + g_wldcore_proposition_data_base[record];
}

/* Expand one 23-byte proposition row into 16-bit fields, then remap the
 * two fee fields through the 16-bit table of record 1. */
void wldcore_unpack_proposition_row(wldcore_proposition_fields_t* out, s32 index) {
    u8* rows = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_ROWS);
    u16* field_lookup;
    s32 i;
    u16* fields = &out->category_index;

    for (i = 0; i < 23; i++) {
        fields[i] = rows[index * 23 + i];
    }
    field_lookup = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_FEES);
    out->fee_a = field_lookup[out->fee_a];
    out->fee_b = field_lookup[out->fee_b];
}

/* Whether proposition `index` is offered at `location`: the row's location
 * (0 for any), then each condition its flags select. The status condition
 * tests bit 2 of script variable 0x360 + condition_value. */
s32 wldcore_proposition_is_available_at_location(s32 index, s32 location) {
    wldcore_proposition_fields_t entry;
    s32 month;
    s32 day;

    wldcore_unpack_proposition_row(&entry, index);
    if (entry.location != 0) {
        if (location != entry.location) {
            return 0;
        }
    }
    if (entry.condition_flags & WLDCORE_PROPOSITION_CONDITION_SCRIPT_VAR_6F) {
        if (script_get_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) < entry.required_script_var_6f) {
            return 0;
        }
    }
    if (entry.condition_flags & WLDCORE_PROPOSITION_CONDITION_MONTH) {
        month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
        day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
        wldcore_calendar_convert_date_to_zodiac(&month, &day);
        if (month != entry.condition_value) {
            return 0;
        }
    }
    if (entry.condition_flags & WLDCORE_PROPOSITION_CONDITION_STATUS) {
        if ((script_get_variable(entry.condition_value + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
                & WLDCORE_PROPOSITION_VARIABLE_COMPLETED)
            == 0) {
            return 0;
        }
    }
    return 1;
}

/*
 * Advance the calendar by 1 day and run the daily updates.
 *
 * Script variable 0x67 increases on March 21 and is capped at 99.
 * The target does not validate the incoming month or day.
 */
void wldcore_calendar_advance_day(void) {
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
    wldcore_proposition_advance_daily_counters();
    wldcore_advance_brave_story_birthdays();
    world_unit_update_monster_breeding(1);
}

/* Converts a calendar date in place into a zodiac sign number (1 = Aries) and
 * the day within that sign. Invalid dates play sound effect 5 and reset to 1. */
void wldcore_calendar_convert_date_to_zodiac(s32* month_ptr, s32* day_ptr) {
    s32 index = 0;
    s32 delta;
    s32 previous_month;
    calendar_date_t* start_dates;
    u8* start_days;
    calendar_date_t* previous_dates;

    if (*month_ptr < 1 || *month_ptr > 12 || *day_ptr < 1 || *day_ptr > 31) {
        wldcore_sound_play_effect(MAIN_SFX_INVALID);
        *day_ptr = 1;
        *month_ptr = 1;
        return;
    }
    start_dates = g_wldcore_calendar_zodiac_start_dates;
    while (start_dates[index].month != *month_ptr) {
        index = (index + 1) % 12;
    }
    /* Direct .day indexing folds the two original indexed addresses into one. */
    start_days = &g_wldcore_calendar_zodiac_start_dates[0].day;
    if (*day_ptr < start_days[index * sizeof(calendar_date_t)]) {
        index = (index + 11) % 12;
        *month_ptr = index + 1;
        /* Separate month-table local preserves the original register allocation. */
        previous_dates = g_wldcore_calendar_zodiac_start_dates;
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
 * The daily birthday sweep, run by wldcore_calendar_advance_day after the
 * calendar rolls over: every brave-story character whose stored birthday is
 * today gets a year older, capped at 99.
 *
 * Entry 0 is the player character. Its row in the birthday table is a
 * placeholder (1/1); the real birthday lives in script variables 0x5f (month)
 * and 0x60 (day), written when the player picks one at game start, so entry 0
 * is compared against those two variables and the remaining 63 entries
 * against the table. The table is a 64 x 2 array of month/day byte pairs
 * reached through the pointer cell at 0x80096a48 (it points at 0x80097028,
 * whose 128 bytes read as plausible month/day pairs throughout).
 *
 * The age column is walked one byte at a time from index 1, which is why the
 * loop's base materialises as the biased constant 0x80057f35; entry 0 is the
 * only absolute access to 0x80057f34.
 */
static void wldcore_advance_brave_story_birthdays(void) {
    const u8(*birthdays)[2] = g_wldcore_brave_story_birthdays;
    s32 day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
    s32 month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    s32 birth_month = script_get_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH);
    s32 birth_day = script_get_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY);
    u8 age;
    s32 i;

    if (month == birth_month && day == birth_day) {
        age = g_main_brave_story_character_ages[0];
        if (age < 99) {
            g_main_brave_story_character_ages[0] = age + 1;
        }
    }

    for (i = 1; i < 64; i++) {
        if (month == birthdays[i][0] && day == birthdays[i][1]) {
            if (g_main_brave_story_character_ages[i] < 99) {
                g_main_brave_story_character_ages[i]++;
            }
        }
    }
}

/* WLDCORE's wrapper for main_party_get_unit: the roster record of slot `party_index`. */
party_unit_t* wldcore_get_party_unit(s32 party_index) {
    return main_party_get_unit(party_index);
}

/* Writes one nine-bit record: bits 0-4 from source->day, bits 5-8 from
 * source->month. Records are packed nine bits apart in the bit array. */
void wldcore_write_nine_bit_record(u32* bits, s32 record, calendar_date_t* source) {
    s32 base = record * 9;
    s32 mask;
    s32 i;

    mask = 1;
    for (i = 0; i < 5; i++) {
        wldcore_set_bit_value(bits, base + i, source->day & mask);
        mask <<= 1;
    }
    mask = 1;
    for (i = 0; i < 4; i++) {
        /* The separate temporary keeps the target's add order: folding
         * base + (i + 5) in one expression hoists base + 5 out of the loop. */
        s32 index = i + 5;

        wldcore_set_bit_value(bits, base + index, source->month & mask);
        mask <<= 1;
    }
}

/* Reads one nine-bit record: bits 0-4 into dest->day, bits 5-8 into dest->month.
 * Records are packed nine bits apart in the bit array, matching
 * wldcore_write_nine_bit_record. A field that decodes to zero or beyond its
 * range falls back to 1. */
void wldcore_read_nine_bit_record(u32* bits, s32 record, calendar_date_t* dest) {
    s32 base = record * 9;
    s32 mask;
    s32 i;

    mask = 1;
    dest->day = 0;
    for (i = 0; i < 5; i++) {
        if (wldcore_test_bit(bits, base + i)) {
            dest->day |= mask;
        }
        mask <<= 1;
    }
    mask = 1;
    dest->month = 0;
    for (i = 0; i < 4; i++) {
        s32 index = i + 5;

        if (wldcore_test_bit(bits, base + index)) {
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
 * asked, zero the 0x7800-entry scratch buffer. */
void wldcore_gfx_clear_vram_and_scratch(s32 clear_scratch) {
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
        for (i = 0x77ff; i >= 0; i--) {
            g_wldcore_scratch_buffer[i] = 0;
        }
    }
    DrawSync(0);
}
