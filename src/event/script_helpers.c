/* Inflict status commands, unit animation and facing, music volume, screen overlay ramps, modal menu sound effects,
 * camera angle helpers, event input, sprite and window buffer init, texture rect allocation and the menu buffer
 * allocator. */

#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/camera.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_window.h"
#include "fft/message.h"
#include "fft/portrait.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/status.h"
#include "fft/status_screen.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"
#ifdef FFT_WORLD
#    include "fft/screen.h"
#endif

/* Menu palette color of each of the numeric display frame's twelve lines. */
static u8 g_status_screen_numbers_palette_indices[12] = { 1, 1, 15, 7, 3, 2, 7, 14, 13, 13, 13, 13 };

/* Twelve menu_line_layout_t records, walked by byte offset. */
static s8 g_status_screen_numbers_frame_offsets[96]
    = { 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, -1, 0, 0, 0, 1, 0, 1, -2, 1, 0, 0, 1, 0, 1, 1, 1, -2, 0, 0, 0, 1, 1, -1, 1, -1,
          0, 1, 0, 1, -1, 2, -1, -2, 1, 0, 1, 1, -1, 1, -1, 1, 1, 0, 1, 0, 1, -1, -1, -1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1,
          1, 1, 1, 2, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, -1, 1, 0, 1, 1 };

static void menu_clear_window_buffer_pair_by_thread(s32 thread_id);

/* As an event starts in battle, run each of its InflictStatus instructions whose unit
 * has Float or Jump, one at a time on a free thread. */
void script_process_inflict_status_commands(void) {
    s32 instruction_offset = 0;
    s32 runtime_id;
    s32 battle_id;
    s32 thread_id;
    const u8* instruction;
    unit_stats_t* unit;

    while ((instruction_offset = script_find_instruction_byte_offset(instruction_offset, EVENT_OPCODE_INFLICT_STATUS))
        != 0) {
        instruction = (const u8*)g_script_event_block;
        if (instruction[instruction_offset] == EVENT_OPCODE_INFLICT_STATUS) {
            runtime_id = script_resolve_runtime_id(script_read_s16(instruction + instruction_offset + 1));
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_id = unit_get_battle_id_by_runtime_id(runtime_id);
                /* EVENT_BATTLE_ID_NONE is unit_get_battle_id_by_event_unit_id's "no unit" value;
                 * this lookup fails with -1 instead, so the test never rejects anything. */
                if (battle_id != EVENT_BATTLE_ID_NONE) {
                    unit = unit_get_stats(battle_id);
                    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
                            & STATUS_BYTE_MASK(STATUS_ID_FLOAT))
                        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
                            & STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                        thread_id = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
                        thread_start(thread_id, script_inflict_status_thread);
                        thread_set_parameters(
                            thread_id, (s32)((const u8*)g_script_event_block + instruction_offset + 1), 0, 0);
                        thread_wait_until_inactive(thread_id);
                    }
                }
            }
        }
        instruction_offset += g_script_event_instruction_sizes[EVENT_OPCODE_INFLICT_STATUS] + 1;
    }
}

/* Set a unit's animation. Once the battle has started and script variable
 * EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY is set, a unit that rides or carries
 * another gets its status animation instead, and a unit standing in water of depth 2
 * or more its mounted animation; both run on the main stack. Animations from
 * ANIMATION_SEQUENCE_EVTCHR_SLOT_0 up are always played as requested. */
void unit_set_animation(s32 runtime_id, u32 animation) {
    s32 battle_id;
    unit_stats_t* stats;
    s32 depth;

    if ((script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) == 0)
        && (script_get_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) != 0)) {
        battle_id = unit_get_battle_id_by_runtime_id(runtime_id);
        if ((battle_id >= 0) && (animation < ANIMATION_SEQUENCE_EVTCHR_SLOT_0)) {
            stats = unit_get_stats(battle_id);
            depth = battle_map_tile_get_value(
                BATTLE_MAP_TILE_DATA_DEPTH, stats->x, stats->position.y, stats->position.higher_elevation);
            if ((stats->mount_info & UNIT_MOUNT_INFO_ROLE_MASK) != 0) {
                g_thread_call_target = (void (*)(void))animation_set_status_by_runtime_id;
            } else if ((depth & 0xFF) >= 2) {
                g_thread_call_target = (void (*)(void))animation_set_mounted_by_runtime_id;
            } else {
                animation_set_specific_by_runtime_id(runtime_id, animation);
                return;
            }
            thread_call_on_main_stack(runtime_id);
            return;
        }
    }
    animation_set_specific_by_runtime_id(runtime_id, animation);
}

/* Faces a unit, and its mount partner when one is linked, in a direction given in
 * sixteenths of a turn. Script variable EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED
 * suppresses the partner's update. */
void unit_set_facing(s32 runtime_id, s32 facing) {
    s32 battle_id;
    unit_stats_t* stats;

    if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) == 0) {
        battle_id = unit_get_battle_id_by_runtime_id(runtime_id);
        if (battle_id >= 0) {
            stats = unit_get_stats(battle_id);
            if (stats->mount_info != 0) {
                unit_facing_set_sixteenth_by_runtime_id(
                    unit_get_runtime_id_by_battle_id(stats->mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK),
                    facing & 0xFFFF);
            }
        }
    }
    unit_facing_set_sixteenth_by_runtime_id(runtime_id, facing & 0xFFFF);
}

/* Map script volume 0..96 to driver volume 0..127; values above 96 saturate. */
s32 script_scale_music_volume(s32 volume) {
    if (volume == 0) {
        return 0;
    }
    if (volume >= 96) {
        volume = 127;
    } else {
        volume = (volume * 127) / 96;
    }
    return volume;
}

/* Fade the screen in: lower g_gfx_screen_fade from 255 by 4 per frame, yielding once per step, and
 * end on 0. */
void gfx_ramp_screen_overlay_intensity_to_zero(void) {
    g_gfx_screen_fade = 0xFF;
    do {
        thread_yield();
        g_gfx_screen_fade -= 4;
    } while (g_gfx_screen_fade > 0);
    g_gfx_screen_fade = 0;
}

/* Fade the screen out: raise g_gfx_screen_fade from 0 by 4 per frame,
 * yielding once per step, and end on 255. */
void gfx_ramp_screen_overlay_intensity_to_max(void) {
    g_gfx_screen_fade = 0;
    do {
        thread_yield();
        g_gfx_screen_fade += 4;
    } while (g_gfx_screen_fade < 0x100);
    g_gfx_screen_fade = 0xff;
}

/* 1 while the current event id is one of the sixteen tutorial events,
 * 0x19a..0x1a9; WORLD has no tutorial events and always returns 0. */
s32 tutorial_is_current_event(void) {
#ifdef FFT_BATTLE
    return (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) - EVENT_ID_TUTORIAL_FIRST) < 0x10U;
#else
    return 0;
#endif
}

/* The battle id of the unit an event unit id resolves to, or EVENT_BATTLE_ID_NONE (2000) when it
 * resolves to no runtime unit or one without battle stats. */
s32 unit_get_battle_id_by_event_unit_id(s32 event_unit_id) {
    s32 runtime_id = script_resolve_runtime_id(event_unit_id);
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        s32 idx = unit_get_battle_id_by_runtime_id(runtime_id);
        if (idx != -1) {
            return idx;
        }
    }
    return EVENT_BATTLE_ID_NONE;
}

/* Set the flag that menu_clear_modal_open clears: the companion overlay openers (OPTION.OUT and
 * the others) call it before loading their executable. */
void menu_set_modal_open(void) {
    g_menu_modal_open = 1;
}

/* Clears the "an options/menu overlay is on screen" flag. Called from the
 * BATTLE.BIN overlay loaders (EQUIP.OUT, DEBUGCHR.OUT, ...) once the overlay
 * has been torn down. */
void menu_clear_modal_open(void) {
    g_menu_modal_open = 0;
}

/* Queue the confirm sound; the frame update plays the pending effect. */
void sound_queue_sfx_confirm(void) {
    g_sound_pending_sfx_id = SOUND_SFX_CONFIRM;
}

/* Queue the cancel sound; the frame update plays the pending effect. */
void sound_queue_sfx_cancel(void) {
    g_sound_pending_sfx_id = SOUND_SFX_CANCEL;
}

/* Queue the cursor-move sound; the frame update plays the pending effect. */
void sound_queue_sfx_cursor_move(void) {
    g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
}

/* Queue the invalid-choice sound; the frame update plays the pending effect. */
void sound_queue_sfx_invalid(void) {
    g_sound_pending_sfx_id = SOUND_SFX_INVALID;
}

/* Menu thread for a refused command: queue the invalid-choice sound and exit. */
void menu_run_invalid_choice_thread(void) {
    sound_queue_sfx_invalid();
    thread_exit_current();
}

/* Yields the thread at least once, then until the camera stops moving. WORLD's
 * copy calls BATTLE's camera_is_active address (0x8008e17c), which with
 * WLDCORE loaded lies inside travel_find_best_path. */
void camera_wait_until_idle(void) {
    do {
        thread_yield();
    } while (camera_is_active() != 0);
}

/* Reduce the yaw script variable to one turn [0, ONE). Adding ten whole turns first keeps
 * yaws down to -10 turns positive before the mask. */
void camera_normalize_yaw_angle(void) {
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]
        = (g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW] + 0xa000) & 0x0fff;
}

/* Step the camera tilt by 0x10 per frame to the nearer of the two resting tilts, 0x12e
 * (low) or 0x1c0 (high), split at 0x177; then point the map's tilt target at the same
 * resting tilt, zoom the map in and restore the zoom variable to ONE. */
void camera_init_tilt_and_zoom(void) {
    s32 current_tilt = g_script_variables[EVENT_SCRIPT_VAR_CAMERA_TILT];
    s32 target_tilt;
    s32 tilt_step;
    s32 tilt_target_mode = 0;

    if (current_tilt < 0x12e) {
        tilt_step = 0x10;
        target_tilt = 0x12e;
    } else if (current_tilt < 0x177) {
        tilt_step = -0x10;
        target_tilt = 0x12e;
    } else if (current_tilt < 0x1c0) {
        tilt_target_mode = 1;
        tilt_step = 0x10;
        target_tilt = 0x1c0;
    } else {
        tilt_target_mode = 1;
        tilt_step = -0x10;
        target_tilt = 0x1c0;
    }

    if (tilt_step < 0) {
        while (target_tilt < current_tilt) {
            g_script_variables[EVENT_SCRIPT_VAR_CAMERA_TILT] = current_tilt;
            thread_yield();
            current_tilt += tilt_step;
        }
        g_script_variables[EVENT_SCRIPT_VAR_CAMERA_TILT] = target_tilt;
    } else {
        while (current_tilt < target_tilt) {
            g_script_variables[EVENT_SCRIPT_VAR_CAMERA_TILT] = current_tilt;
            thread_yield();
            current_tilt += tilt_step;
        }
        g_script_variables[EVENT_SCRIPT_VAR_CAMERA_TILT] = target_tilt;
    }

    g_camera_tilt_target = tilt_target_mode + 1;
    g_camera_zoom_target = BATTLE_MAP_ZOOM_IN;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM] = ONE;
}

/* Raise negative yaw by whole turns; retain positive values of ONE or greater. */
s32 camera_wrap_yaw_angle(void) {
    s32 yaw = g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW];

    while (yaw < 0) {
        yaw += ONE;
    }
    return yaw;
}

/* Return the stats for the menu's selected battle slot. */
unit_stats_t* menu_get_selected_unit_stats(void) {
    return unit_get_stats(g_unit_panel_selected_unit.battle_id);
}

/* Clamp a signed value, testing the lower bound first. */
void script_clamp_s32_to_range(s32* value, s32 min, s32 max) {
    if (*value < min) {
        *value = min;
        return;
    }
    if (*value > max) {
        *value = max;
    }
}

/* Latch this frame's pad state for the event and menu code and derive the
 * pressed-button mask with key repeat.
 *
 * g_script_event_current_input and _previous_input hold the held buttons.
 * g_script_event_input receives the buttons that count as pressed this
 * frame: the newly pressed ones whenever the pad state changes or no repeatable
 * button (d-pad, L1, R1) is held. A state held unchanged counts frames: at the
 * initial delay the repeatable buttons fire once, and from then on the d-pad
 * fires every repeat period and L1/R1 every secondary period. While Square is
 * held the d-pad repeat is limited to Left/Right, and Up/Down fire with L1/R1 on
 * the secondary period. The three delays (g_main_input_repeat_*) are divided by
 * the event speed.
 *
 * No input at all, or a tutorial event that has Up or Down disabled, resets the
 * three repeat counters. script_route_event_input is the WORLD copy;
 * it takes no argument and fetches the buttons itself. */
void script_update_event_input_state(u32 input) {
    u32 filtered_input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_repeat_delay;
    u32 previous_input;
    u32 counter;

    if (input == 0
        || (tutorial_is_current_event() != 0
            && (script_get_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) & (PSX_PAD_UP | PSX_PAD_DOWN)) != 0)) {
        g_script_event_initial_repeat_counter = 0;
        g_script_event_repeat_counter = 0;
        g_script_event_secondary_repeat_counter = 0;
    }

    previous_input = g_script_event_current_input;
    g_script_event_current_input = input;
    g_script_event_input = 0;
    g_script_event_previous_input = previous_input;
    filtered_input = input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT | PSX_PAD_L1 | PSX_PAD_R1);

    if (previous_input != input || filtered_input == 0) {
        g_script_event_input = ~previous_input & input;
        g_script_event_initial_repeat_counter = 0;
    } else {
        initial_delay = g_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_script_event_initial_repeat_counter;
        if (counter == initial_delay) {
            g_script_event_input = filtered_input;
        }
        if (counter >= initial_delay) {
            counter = ++g_script_event_repeat_counter;
            g_script_event_secondary_repeat_counter++;
            repeat_delay = g_input_repeat_period / g_script_event_speed;
            if (counter >= repeat_delay) {
                g_script_event_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_script_event_input = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_script_event_input = input & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT);
                }
            }
            secondary_repeat_delay = g_input_secondary_repeat_period;
            {
                s32 event_speed;
                event_speed = g_script_event_speed;
                secondary_repeat_delay /= event_speed;
            }
            if (g_script_event_secondary_repeat_counter >= secondary_repeat_delay) {
                g_script_event_secondary_repeat_counter = 0;
                if ((g_script_event_current_input & PSX_PAD_SQUARE) != 0) {
                    g_script_event_input
                        |= g_script_event_current_input & (PSX_PAD_UP | PSX_PAD_DOWN | PSX_PAD_L1 | PSX_PAD_R1);
                } else {
                    g_script_event_input |= g_script_event_current_input & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_script_event_repeat_counter = 0;
            g_script_event_secondary_repeat_counter = 0;
        }
    }
}

/* Empty in BATTLE and WORLD; menu_init_subsystems calls it. */
void menu_init_stub(void) {
}

/* Initialize a menu quad off-screen at x=-512, with neutral shading,
 * a 4-bit texture page at VRAM (448, 0) and the menu CLUT at (960, 496). */
void menu_init_default_poly_ft4(POLY_FT4* poly) {
    SetPolyFT4(poly);
    SetShadeTex(poly, 0);
    setRGB0(poly, 0x80, 0x80, 0x80);
    setUV4(poly, 0, 0, 1, 0, 0, 1, 1, 1);
    setXY4(poly, -0x200, 0, -0x200, 0, -0x200, 0, -0x200, 0);
    poly->tpage = GetTPage(0, 0, 0x1c0, 0);
    poly->clut = getClut(0x3c0, 0x1f0);
}

/* Initialize the fixed sprite of the menu graphics; only BATTLE shades its texture. */
void menu_init_sprt(SPRT* sprite) {
    SetSprt(sprite);
#ifdef FFT_BATTLE
    SetShadeTex(sprite, 1);
#else
    SetShadeTex(sprite, 0);
#endif
    setRGB0(sprite, 0x80, 0x80, 0x80);
    sprite->x0 = 0x200;
    sprite->u0 = 0;
    sprite->v0 = 0;
    sprite->w = 0;
    sprite->h = 0;
    sprite->y0 = 0;
    sprite->clut = MENU_CLUT_NORMAL;
    SetSemiTrans(sprite, 1);
}

/* menu_init_sprt for count consecutive sprites, each then given clut. */
void menu_init_sprite_array(SPRT* sprites, s32 count, s32 clut) {
    s32 index;

    for (index = 0; index < count; index++) {
        menu_init_sprt(sprites);
        sprites->clut = clut;
        sprites++;
    }
}

/* Mark all three menu-buffer pointer pairs as unallocated. */
void menu_reset_window_buffer_slots(void) {
    menu_window_buffer_t* value = (menu_window_buffer_t*)-1;
    s32 index;

    for (index = 5; index >= 0; index--) {
        g_menu_window_buffer_slots[index] = value;
    }
}

/* Claim the first free pair of the three window buffer pairs (one record per packet buffer)
 * for the current thread and return its first record. The retail loop runs once, so only
 * the first record's sprites, draw mode and quads are initialized here. When no pair is free
 * the current thread stops.
 *
 * CLUT ids encode (y << 6) | (x >> 4): the ones below are palettes at x = 960, rows
 * 498-505. */
menu_window_buffer_t* menu_alloc_window_buffer_pair(void) {
    menu_window_buffer_t* buffer;
    s32 index;
    s32 i;

    for (index = 0; index < 6; index += 2) {
        if (g_menu_window_buffer_slots[index] == (menu_window_buffer_t*)-1) {
            g_menu_window_buffer_slots[index] = &g_menu_window_buffers[index];
            g_menu_window_buffer_slots[index + 1] = &g_menu_window_buffers[index + 1];
            buffer = g_menu_window_buffer_slots[index];
            for (i = 0; i < 1; i++) {
                buffer->active = 0;
                buffer->thread_id = g_thread_current_id;
                buffer->portrait_code = -1;
                buffer->color.r = 0x80;
                buffer->color.g = 0x80;
                buffer->color.b = 0x80;
                buffer->dialogue_selector = 0;
                buffer->portrait_flipped = 0;
                buffer->is_message_box = 0;
                buffer->icon_flags = 0;
                buffer->scroll_range = 1;
                buffer->scroll_position = 1;
                menu_init_sprite_array(buffer->sprites, 4, MENU_CLUT_NORMAL);
                menu_init_sprite_array(buffer->icon_sprites, 3, 0x7e7c);
                SetDrawMode(&buffer->draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);
                SetSemiTrans(&buffer->sprites[0], 0);
                SetSemiTrans(&buffer->sprites[1], 1);
                buffer->sprites[0].u0 = 0xa8;
                buffer->sprites[1].u0 = 0xb8;
                buffer->sprites[0].clut = 0x7d7c;
                buffer->sprites[1].clut = 0x7dbc;
                buffer->sprites[2].clut = 0x7cbc;
                buffer->icon_sprites[0].u0 = 0xd8;
                buffer->sprites[0].w = 0x10;
                buffer->sprites[0].h = 0x10;
                buffer->sprites[1].w = 0x10;
                buffer->sprites[1].h = 0x10;
                buffer->sprites[2].w = 0x10;
                buffer->sprites[2].h = 0x10;
                buffer->sprites[0].v0 = 0;
                buffer->sprites[1].v0 = 0;
                buffer->icon_sprites[0].w = 8;
                buffer->icon_sprites[0].h = 0x10;
                buffer->icon_sprites[1].w = 8;
                buffer->icon_sprites[1].h = 0x10;
                buffer->icon_sprites[2].w = 8;
                buffer->icon_sprites[2].h = 8;
                buffer->icon_sprites[0].v0 = 0;
                buffer->icon_sprites[1].u0 = 0xe0;
                buffer->icon_sprites[1].v0 = 0;
                buffer->icon_sprites[2].u0 = 0x10;
                buffer->icon_sprites[2].v0 = 0x10;
                menu_init_default_poly_ft4(&buffer->quads[0]);
                menu_init_default_poly_ft4(&buffer->quads[1]);
                buffer->quads[0].tpage = GetTPage(0, 0, 0x3c0, 0x100);
                buffer->quads[1].tpage = GetTPage(0, 0, 0x340, 0x100);
                buffer++;
            }
            return g_menu_window_buffer_slots[index];
        }
    }
    thread_exit_current();
}

/* Clear the pair whose first record's thread matches; the target forms one
 * 0x118-biased induction value across both records (compare 0x800e27f4).
 * BATTLE's copy is empty and never called. */
static void menu_clear_window_buffer_pair_by_thread(s32 thread_id) {
#ifdef FFT_WORLD
    s32 index;

    for (index = 0; index < 6; index += 2) {
        if (g_menu_window_buffers[index].thread_id == thread_id) {
            g_menu_window_buffers[index].active = 0;
            g_menu_window_buffers[index + 1].active = 0;
            return;
        }
    }
#endif
}

/* Release a pair handed out by menu_alloc_window_buffer_pair: free both slots and mark both
 * records inactive and unowned. -1 is ignored; a pointer that is not a pair's first record stops
 * the current thread. */
void menu_release_window_buffer_pair(menu_window_buffer_t* buffer) {
    s32 slot;
    menu_window_buffer_t* empty;

    if (buffer == (menu_window_buffer_t*)-1) {
        return;
    }

    slot = 0;
    empty = (menu_window_buffer_t*)-1;
    for (; slot < 6; slot += 2) {
        if (g_menu_window_buffer_slots[slot] == buffer) {
            g_menu_window_buffer_slots[slot] = empty;
            g_menu_window_buffer_slots[slot + 1] = empty;
            g_menu_window_buffers[slot].active = 0;
            g_menu_window_buffers[slot + 1].active = 0;
            g_menu_window_buffers[slot].thread_id = 0;
            g_menu_window_buffers[slot + 1].thread_id = 0;
            return;
        }
    }

    thread_exit_current();
}

/* Set the color in a GPU primitive's common header to one gray level. */
static void gfx_set_primitive_brightness(P_TAG* primitive, s32 brightness) {
    setRGB0(primitive, brightness, brightness, brightness);
}

/* Draws the active menu windows, last slot pair first. A window is drawn
 * while its buffer is active and its owner thread runs; BATTLE scales its
 * frame primitives by the window's own brightness, WORLD by the one menu
 * brightness, and WORLD dims a message box while a menu task is active. The
 * scroll icons sit 15 pixels in from the right edge (13 when the width is not
 * 4 more than a multiple of 8), shifted 4 pixels up or down for a portrait
 * box, and the scroll marker lies between them in proportion to
 * scroll_position / scroll_range. The portrait quad takes its texture from
 * portrait_code, or in BATTLE from the unit's portrait when that is -1. */
#ifdef FFT_WORLD
#    define WINDOW_BRIGHTNESS g_screen_color_red
#else
#    define WINDOW_BRIGHTNESS window->color.r
#endif
void menu_draw_active_window_frames(void) {
    menu_window_buffer_t* window;
    menu_window_buffer_t* buffer;
    s32 height;
    s32 i;
    s32 offset;
    s32 top;
    s32 span;
    s32 flags;
    s32 bottom;
#ifdef FFT_BATTLE
    s32 u;
#endif

    for (i = 4; i >= 0; i -= 2) {
        if (g_menu_window_buffer_slots[i] == (menu_window_buffer_t*)-1) {
            continue;
        }
        buffer = g_menu_window_buffer_slots[i + g_menu_packet_buffer_index];
        if (buffer->active != 1 || g_thread_contexts[buffer->thread_id].is_running == 0) {
            continue;
        }
        window = buffer;
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[0], WINDOW_BRIGHTNESS);
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[1], WINDOW_BRIGHTNESS);
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[2], WINDOW_BRIGHTNESS);
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[3], WINDOW_BRIGHTNESS);
        gfx_set_primitive_brightness((P_TAG*)&window->quads[0], WINDOW_BRIGHTNESS);
        gfx_set_primitive_brightness((P_TAG*)&window->quads[1], WINDOW_BRIGHTNESS);
#ifdef FFT_WORLD
        if (window->is_message_box != 0) {
            if (g_menu_input_lock_mode != 0) {
                window->quads[0].clut = MENU_CLUT_DIMMED;
                setRGB0(&window->quads[1], 0x40, 0x50, 0x70);
            } else {
                window->quads[0].clut = MENU_CLUT_NORMAL;
            }
        }
#endif
        offset = window->quads[0].x1 - window->quads[0].x0;
        if (offset / 8 * 8 == offset - 4) {
            offset -= 15;
        } else {
            offset -= 13;
        }
        height = window->quads[0].y2 - window->quads[0].y0;
        window->icon_sprites[0].x0 = window->quads[0].x0 + offset;
        window->icon_sprites[0].y0 = window->quads[0].y0 + 8;
        window->icon_sprites[1].x0 = window->quads[0].x0 + offset;
        window->icon_sprites[1].y0 = window->quads[0].y0 + height - 0x1A;
        if (window->box_type == DIALOG_BOX_TYPE_PORTRAIT) {
            if (window->dialogue_selector == 1) {
                window->icon_sprites[0].y0 -= 4;
                window->icon_sprites[1].y0 -= 4;
            } else {
                window->icon_sprites[0].y0 += 4;
                window->icon_sprites[1].y0 += 4;
            }
        }
        bottom = window->icon_sprites[1].y0;
        top = window->icon_sprites[0].y0 + window->icon_sprites[0].h;
        bottom -= top;
        /* Reload the pointer cell: retaining buffer removes target loads. */
        span = (bottom - 4) * g_menu_window_buffer_slots[i + g_menu_packet_buffer_index]->scroll_position
            / g_menu_window_buffer_slots[i + g_menu_packet_buffer_index]->scroll_range;
        window->icon_sprites[2].x0 = window->quads[0].x0 + offset;
        window->icon_sprites[2].y0 = top + span;
        flags = g_menu_window_buffer_slots[i + g_menu_packet_buffer_index]->icon_flags;
        if (flags & MENU_WINDOW_ICON_SCROLL_UP) {
            gfx_draw_or_append_gpu_primitive(&window->icon_sprites[0]);
        }
        if (flags & MENU_WINDOW_ICON_SCROLL_DOWN) {
            gfx_draw_or_append_gpu_primitive(&window->icon_sprites[1]);
        }
        if (flags & MENU_WINDOW_ICON_SCROLL_MARKER) {
            gfx_draw_or_append_gpu_primitive(&window->icon_sprites[2]);
        }
        gfx_draw_or_append_gpu_primitive(&window->sprites[0]);
        gfx_draw_or_append_gpu_primitive(&window->sprites[1]);
        gfx_draw_or_append_gpu_primitive(&window->sprites[2]);
        gfx_draw_or_append_gpu_primitive(&window->sprites[3]);
        gfx_draw_or_append_gpu_primitive(&window->draw_mode);
        if (window->portrait_code != -1) {
#ifdef FFT_BATTLE
            /* Portrait slot n is 32 x 48 pixels at u = 32 n of the portrait page, with its
             * CLUT at x = 0x1C0 + 16 (n % 4), y = 0xFE + n / 4. */
            u = window->portrait_code << 5;
            setUV4(&window->quads[1], u, 0xCE, u + 0x1F, 0xCE, u, 0xFE, u + 0x1F, 0xFE);
            window->quads[1].tpage = GetTPage(0, 0, 0x1C0, 0);
            window->quads[1].clut = GetClut(window->portrait_code % 4 * 16 + 0x1C0, window->portrait_code / 4 + 0xFE);
#else
            portrait_build_unit_poly(&window->quads[1], window->portrait_code);
#endif
        }
#ifdef FFT_BATTLE
        else {
            portrait_build_unit_poly(&window->quads[1], window->portrait_battle_id);
        }
        gfx_flip_poly_ft4_uv_if_negative(&window->quads[1], -window->portrait_flipped);
#endif
        gfx_draw_or_append_gpu_primitive(&window->quads[1]);
        gfx_draw_or_append_gpu_primitive(&window->quads[0]);
    }
}

/* Free every cell of the 16 x 15 texture allocation grid that gfx_alloc_texture_rect claims from. */
void gfx_clear_texture_allocation_grid(void) {
    s32 row;
    s32 column;

    for (row = 0; row < 15; row++) {
        for (column = 15; column >= 0; column--) {
            g_gfx_texture_allocation_grid[row][column] = 0;
        }
    }
}

/* Allocate a texture rectangle in the 16-column VRAM allocation grid.
 *
 * The rect size rounds up to 16-pixel cells. The first free position whose
 * cells are all unclaimed is marked 0xff, the primitive UVs/CLUT (and TPage
 * for POLY_FT4) are pointed at it, and rect is rewritten to the 4bpp VRAM
 * area at x 0x1c0 before image (unless -1) is uploaded. A full grid stops
 * this thread. The allocation grid is local to the active scene. */
void gfx_alloc_texture_rect(RECT* rect, void* prim, u32* image) {
    s16 width = rect->w;
    s16 height = rect->h;
    s32 width_cells = width >> 4;
    s32 height_cells = height >> 4;
    s32 row;
    s32 column;
    s32 y;
    s32 x;

    if (width & 0xF) {
        width_cells++;
    }
    if (height & 0xF) {
        height_cells++;
    }
    for (row = 0; row < 16 - height_cells; row++) {
        for (column = 0; column <= 16 - width_cells; column++) {
            if (g_gfx_texture_allocation_grid[row][column] != 0) {
                continue;
            }
            for (y = row; y < row + height_cells; y++) {
                for (x = column; x < column + width_cells; x++) {
                    if (g_gfx_texture_allocation_grid[y][x] == 0xFF) {
                        goto next;
                    }
                }
            }
            for (y = row; y < row + height_cells; y++) {
                for (x = column; x < column + width_cells; x++) {
                    g_gfx_texture_allocation_grid[y][x] = 0xFF;
                }
            }
            x = column * 16;
            y = row * 16;
            /* GPU codes 0x64-0x67 are the free-size sprite (SPRT). */
            if ((getcode(prim) & 0xFC) == 0x64) {
                SPRT* sprite = prim;

                sprite->u0 = x;
                sprite->v0 = y;
                sprite->w = rect->w;
                sprite->h = rect->h;
                sprite->clut = MENU_CLUT_NORMAL;
            } else {
                POLY_FT4* quad = prim;

                quad->u0 = quad->u2 = x;
                quad->v0 = quad->v1 = y;
                /* Keep the right edge u inside the 256-pixel page. */
                if (x + rect->w >= 0x100) {
                    x--;
                }
                quad->u1 = quad->u3 = rect->w + x;
                quad->v2 = quad->v3 = rect->h + y;
                quad->tpage = GetTPage(0, 0, 0x1C0, 0);
                quad->clut = MENU_CLUT_NORMAL;
            }
            rect->x = column * 4 + 0x1C0;
            rect->y = y;
            rect->w = (rect->w & 3) ? (rect->w >> 2) + 1 : rect->w >> 2;
            if (image != (u32*)-1) {
                LoadImage(rect, image);
            }
            return;
        next:;
        }
    }
    thread_exit_current();
}

/* Release occupied cells. The rectangle is read before its pointer sentinel
 * is checked; out-of-range positive cell coordinates stop this thread. */
void gfx_free_texture_rect(RECT* rect) {
    s32 x;
    s32 y;
    if (rect->x != -1 && rect != (void*)-1) {
        g_gfx_free_rect_pixels[0] = (rect->x - 0x1c0) * 4;
        g_gfx_free_rect_pixels[1] = rect->y & 0xff;
        g_gfx_free_rect_pixels[2] = rect->w * 4;
        g_gfx_free_rect_pixels[3] = rect->h;
        texture_rect_pixels_to_cells(g_gfx_free_rect_pixels, g_gfx_free_rect_cells);
        for (y = g_gfx_free_rect_cells[1]; y < g_gfx_free_rect_cells[1] + g_gfx_free_rect_cells[3]; y++) {
            for (x = g_gfx_free_rect_cells[0]; x < g_gfx_free_rect_cells[0] + g_gfx_free_rect_cells[2]; x++) {
                if (x >= 16 || y >= 15) {
                    thread_exit_current();
                }
                g_gfx_texture_allocation_grid[y][x] = 0;
            }
        }
    }
}

/* Reset the menu arena allocator: every allocation slot unused (-1), every arena page free, and
 * the three typewriter glyph slots unowned. */
void menu_init_buffer_allocator(void) {
    s32 fill;
    s32 slot;
    s32 count;
    u8* byte;
    s32* first;
    s32* second;

    /* One counter local shared by the last two loops and each counter set
     * before its pointer: this reproduces the target's register assignment
     * without pins. */
    fill = -1;
    for (slot = MENU_BUFFER_ALLOCATION_COUNT - 1; slot >= 0; slot--) {
        g_menu_allocator_slot_records[slot].buffer = (u8*)fill;
    }
    count = VARIANT_MENU_BUFFER_PAGE_COUNT - 1;
    byte = &g_menu_buffer_allocation_map[VARIANT_MENU_BUFFER_PAGE_COUNT - 1];
    for (; count >= 0; count--) {
        *byte = 0;
        byte--;
    }
    count = 0;
    second = g_text_slot_glyph_counts;
    first = g_text_slot_owner_thread_ids;
    for (; count < 3; count++) {
        *first = 0;
        *second = 0;
        second++;
        first++;
    }
}

/* Claims enough 256-byte arena pages for size bytes and records them in a free slot.
 *
 * Scans the page occupancy map first-fit. When no run of free pages or no free slot
 * remains, it optionally prints a diagnostic, yields the thread, and rescans.
 * The arena holds VARIANT_MENU_BUFFER_PAGE_COUNT pages (53 in BATTLE, 112 in WORLD). */
void* menu_alloc_memory(s32 size) {
    s32 page;
    s32 pages;
    s32 i;
    u8* buffer;

    size = (size & 0xFF) != 0 ? (size >> 8) + 1 : size >> 8;
    for (;;) {
        for (page = 0; page < VARIANT_MENU_BUFFER_PAGE_COUNT; page++) {
            if (g_menu_buffer_allocation_map[page] == 0) {
                for (pages = 0;; pages++) {
                    if (pages + page >= VARIANT_MENU_BUFFER_PAGE_COUNT) {
                        /* Leaves the run and page loops for the retry wait. */
                        goto fail;
                    }
                    if (pages >= size || g_menu_buffer_allocation_map[pages + page] != 0) {
                        break;
                    }
                }
                if (pages >= size) {
                    for (pages = 0; pages < size; pages++) {
                        g_menu_buffer_allocation_map[page + pages] = 1;
                    }
                    for (i = 0; i < MENU_BUFFER_ALLOCATION_COUNT; i++) {
                        if (g_menu_allocator_slot_records[i].buffer == (u8*)-1) {
                            buffer = g_menu_buffer_arena[page];
                            g_menu_allocator_slot_records[i].buffer = buffer;
                            g_menu_allocator_slot_records[i].page_count = pages;
                            return buffer;
                        }
                    }
                    break;
                }
            }
        }
    fail:
        if (g_debug_font_print_enabled != 0) {
            FntPrint(g_message_wait_for_allocation_message, g_thread_current_id, size);
        }
        thread_yield();
    }
}

/* Thin public entry point for the BATTLE menu-buffer allocator. */
void* menu_alloc_buffer(s32 bytes) {
    return menu_alloc_memory(bytes);
}

/* Free a menu arena allocation: release its slot record and clear its pages in the map.
 * Pointers at or above the companion overlay load address, and -1, are ignored; any other
 * pointer that no slot records stops the current thread. */
void menu_free_memory(void* buffer_pointer) {
    u32 buffer = (u32)buffer_pointer;
    u32 grid_base;
    s32* allocation_size;
    menu_allocation_t* allocation;
    s32* allocation_end;
    u8* allocation_grid;
    s32 invalid_buffer;
    s32 page;

    if (buffer < (u32)g_heap_companion_overlay_load_address && buffer != -1) {
        invalid_buffer = -1;
        grid_base = (buffer - (u32)g_menu_buffer_arena) >> 8;
        allocation_grid = g_menu_buffer_allocation_map;
        /* A separate page-count cursor preserves the native count-field base. */
        allocation_size = &g_menu_allocator_slot_records[0].page_count;
        allocation = g_menu_allocator_slot_records;
        allocation_end = &g_menu_allocator_slot_records[MENU_BUFFER_ALLOCATION_COUNT].page_count;
        for (;;) {
            if ((u32)allocation->buffer == buffer) {
                allocation->buffer = (u8*)invalid_buffer;
                /* Written as (index + base) + grid so the two addu operands
                 * come out in the target's order; the size is reloaded
                 * each iteration because the byte store may alias it. */
                for (page = 0; page < *allocation_size; page++) {
                    *(u8*)((page + grid_base) + (u32)allocation_grid) = 0;
                }
                return;
            }
            allocation_size += sizeof(menu_allocation_t) / sizeof(*allocation_size);
            allocation++;
            if ((s32)allocation_size >= (s32)allocation_end) {
                thread_exit_current();
                return;
            }
        }
    }
}

/* Build the numeric display frame around box: twelve lines placed by
 * g_status_screen_numbers_frame_offsets relative to the box's corners and colored from
 * the normal menu palette, the two draw modes and a sprite covering the box. */
void status_screen_init_numbers_frame(RECT* box, menu_palette_primitives_t* menu) {
    LINE_F2* line = menu->lines;
    s32 i;
    s32 offset;

    for (i = 0; i < 12; i++) {
        SetLineF2(line);
        line++;
    }
    menu_init_primitive_colors_normal(menu);
    line = menu->lines;
    offset = 0;
    for (i = 0; i < 12; i++) {
        line->x0 = g_status_screen_numbers_frame_offsets[offset + MENU_LINE_LAYOUT_X0] + box->x;
        line->y0 = g_status_screen_numbers_frame_offsets[offset + MENU_LINE_LAYOUT_Y0] + box->y;
        line->x1 = g_status_screen_numbers_frame_offsets[offset + MENU_LINE_LAYOUT_X1] + box->x;
        line->y1 = g_status_screen_numbers_frame_offsets[offset + MENU_LINE_LAYOUT_Y1] + box->y;
        if (g_status_screen_numbers_frame_offsets[offset + MENU_LINE_LAYOUT_ADD_W0] != 0) {
            line->x0 += box->w;
        }
        if (g_status_screen_numbers_frame_offsets[offset + MENU_LINE_LAYOUT_ADD_H0] != 0) {
            line->y0 += box->h;
        }
        if (g_status_screen_numbers_frame_offsets[offset + MENU_LINE_LAYOUT_ADD_W1] != 0) {
            line->x1 += box->w;
        }
        if (g_status_screen_numbers_frame_offsets[offset + MENU_LINE_LAYOUT_ADD_H1] != 0) {
            line->y1 += box->h;
        }
        offset += MENU_LINE_LAYOUT_SIZE;
        line++;
    }
    SetDrawMode(&menu->draw_mode_menu, 1, 0, GetTPage(0, 0, 0x3c0, 0x100), &g_status_screen_numbers_texture_window);
    SetDrawMode(&menu->draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);
    menu_init_sprt(&menu->sprite);
    menu->sprite.x0 = box->x;
    menu->sprite.y0 = box->y;
    menu->sprite.w = box->w;
    menu->sprite.h = box->h;
}

/* Submit the frame's twelve lines, sprite and draw modes. */
void status_screen_submit_numbers_frame(menu_palette_primitives_t* display) {
    s32 index;

    for (index = 0; index < 12; index++) {
        gfx_draw_or_append_gpu_primitive(&display->lines[index]);
    }
    gfx_draw_or_append_gpu_primitive(&display->draw_mode);
    gfx_draw_or_append_gpu_primitive(&display->sprite);
    gfx_draw_or_append_gpu_primitive(&display->draw_mode_menu);
}

/* Give the twelve frame lines the colors of the normal menu palette (the first
 * sixteen of the menu palette colors) and the sprite the normal menu CLUT, GetClut(0x3c0, 0x1f0). */
void menu_init_primitive_colors_normal(menu_palette_primitives_t* menu) {
    s32 index;
    s32 palette_index;
    LINE_F2* line;

    for (index = 0, line = menu->lines; index < 12; index++, line++) {
        palette_index = g_status_screen_numbers_palette_indices[index];
        line->r0 = g_menu_palette_colors.colors[palette_index].red << 3;
        line->g0 = g_menu_palette_colors.colors[palette_index].green << 3;
        line->b0 = g_menu_palette_colors.colors[palette_index].blue << 3;
    }
    menu->sprite.clut = MENU_CLUT_NORMAL;
}

/* Give the twelve frame lines the colors of the dimmed menu palette (the second
 * sixteen of the menu palette colors) and the sprite the dimmed menu CLUT, GetClut(0x3c0, 0x1f4). */
void menu_init_primitive_colors_dimmed(menu_palette_primitives_t* menu) {
    s32 index;
    s32 palette_index;
    LINE_F2* line;

    for (index = 0, line = menu->lines; index < 12; index++, line++) {
        palette_index = (g_status_screen_numbers_palette_indices[index] + PALETTE_COLOR_COUNT);
        line->r0 = g_menu_palette_colors.colors[palette_index].red << 3;
        line->g0 = g_menu_palette_colors.colors[palette_index].green << 3;
        line->b0 = g_menu_palette_colors.colors[palette_index].blue << 3;
    }
    menu->sprite.clut = MENU_CLUT_DIMMED;
}
