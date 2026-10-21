/* Event unit commands, screen and camera transitions, menu sounds and event input. */

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
#include "fft/portrait.h"
#include "fft/screen.h"
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

/* As an event starts in battle, run each of its InflictStatus instructions whose unit
 * has Float or Jump, one at a time on a free thread. */
void script_status_commands_process(void) {
    s32 instruction_offset = 0;
    s32 runtime_id;
    s32 battle_id;
    s32 thread_id;
    const u8* instruction;
    unit_stats_t* unit;

    while (
        (instruction_offset = script_instruction_offset_find(instruction_offset, EVENT_OPCODE_INFLICT_STATUS)) != 0) {
        instruction = (const u8*)g_script_event_block;
        if (instruction[instruction_offset] == EVENT_OPCODE_INFLICT_STATUS) {
            runtime_id = script_runtime_id_resolve(script_s16_read(instruction + instruction_offset + 1));
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_id = unit_runtime_battle_id_get(runtime_id);
                /* EVENT_BATTLE_ID_NONE is unit_event_battle_id_get's "no unit" value;
                 * this lookup fails with -1 instead, so the test never rejects anything. */
                if (battle_id != EVENT_BATTLE_ID_NONE) {
                    unit = unit_stats_get(battle_id);
                    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
                            & STATUS_BYTE_MASK(STATUS_ID_FLOAT))
                        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
                            & STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                        thread_id = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
                        thread_start(thread_id, script_inflict_status_thread);
                        thread_parameters_set(
                            thread_id, (s32)((const u8*)g_script_event_block + instruction_offset + 1), 0, 0);
                        thread_inactive_wait(thread_id);
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
 * or more uses the standing pose selector; both run on the main stack. Animations from
 * ANIMATION_SEQUENCE_EVTCHR_SLOT_0 up are always played as requested. */
void unit_event_animation_set(s32 runtime_id, u32 animation) {
    s32 battle_id;
    unit_stats_t* stats;
    s32 depth;

    if (script_variable_get(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) == 0
        && script_variable_get(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) != 0) {
        battle_id = unit_runtime_battle_id_get(runtime_id);
        if (battle_id >= 0 && animation < ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
            stats = unit_stats_get(battle_id);
            depth = battle_map_tile_get_value(
                BATTLE_MAP_TILE_DATA_DEPTH, stats->x, stats->position.y, stats->position.higher_elevation);
            if ((stats->mount_info & UNIT_MOUNT_INFO_ROLE_MASK) != 0) {
                g_thread_call_target = (void (*)(void))animation_runtime_status_set;
            } else if ((depth & 0xFF) >= 2) {
                g_thread_call_target = (void (*)(void))animation_runtime_stand_set;
            } else {
                animation_runtime_sequence_set(runtime_id, animation);
                return;
            }
            thread_call_on_main_stack(runtime_id);
            return;
        }
    }
    animation_runtime_sequence_set(runtime_id, animation);
}

/* Faces a unit, and its mount partner when one is linked, in a direction given in
 * sixteenths of a turn. Script variable EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED
 * suppresses the partner's update. */
void unit_event_facing_set(s32 runtime_id, s32 facing) {
    s32 battle_id;
    unit_stats_t* stats;

    if (script_variable_get(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) == 0) {
        battle_id = unit_runtime_battle_id_get(runtime_id);
        if (battle_id >= 0) {
            stats = unit_stats_get(battle_id);
            if (stats->mount_info != 0) {
                unit_runtime_facing_sixteenth_set(
                    unit_battle_runtime_id_get(stats->mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK), facing & 0xFFFF);
            }
        }
    }
    unit_runtime_facing_sixteenth_set(runtime_id, facing & 0xFFFF);
}

/* Map script volume 0..96 to driver volume 0..127; values above 96 saturate. */
s32 script_music_volume_scale(s32 volume) {
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
void gfx_screen_overlay_fade_in(void) {
    g_gfx_screen_fade = 0xFF;
    do {
        thread_yield();
        g_gfx_screen_fade -= 4;
    } while (g_gfx_screen_fade > 0);
    g_gfx_screen_fade = 0;
}

/* Fade the screen out: raise g_gfx_screen_fade from 0 by 4 per frame,
 * yielding once per step, and end on 255. */
void gfx_screen_overlay_fade_out(void) {
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
    return (script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) - EVENT_ID_TUTORIAL_FIRST) < 0x10U;
#else
    return 0;
#endif
}

/* The battle id of the unit an event unit id resolves to, or EVENT_BATTLE_ID_NONE (2000) when it
 * resolves to no runtime unit or one without battle stats. */
s32 unit_event_battle_id_get(s32 event_unit_id) {
    s32 runtime_id = script_runtime_id_resolve(event_unit_id);
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        s32 battle_id = unit_runtime_battle_id_get(runtime_id);
        if (battle_id != -1) {
            return battle_id;
        }
    }
    return EVENT_BATTLE_ID_NONE;
}

/* Set the flag that menu_modal_close clears: the companion overlay openers (OPTION.OUT and
 * the others) call it before loading their executable. */
void menu_modal_open(void) {
    g_menu_modal_open = 1;
}

/* Clears the "an options/menu overlay is on screen" flag. Called from the
 * BATTLE.BIN overlay loaders (EQUIP.OUT, DEBUGCHR.OUT, ...) once the overlay
 * has been torn down. */
void menu_modal_close(void) {
    g_menu_modal_open = 0;
}

/* Queue the confirm sound; the frame update plays the pending effect. */
void sound_confirm_queue(void) {
    g_sound_pending_sfx_id = SOUND_SFX_CONFIRM;
}

/* Queue the cancel sound; the frame update plays the pending effect. */
void sound_cancel_queue(void) {
    g_sound_pending_sfx_id = SOUND_SFX_CANCEL;
}

/* Queue the cursor-move sound; the frame update plays the pending effect. */
void sound_cursor_queue(void) {
    g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
}

/* Queue the invalid-choice sound; the frame update plays the pending effect. */
void sound_invalid_queue(void) {
    g_sound_pending_sfx_id = SOUND_SFX_INVALID;
}

/* Menu thread for a refused command: queue the invalid-choice sound and exit. */
void menu_invalid_choice_thread_run(void) {
    sound_invalid_queue();
    thread_current_exit();
}

/* Yields the thread at least once, then until the camera stops moving. WORLD's
 * copy calls BATTLE's camera_is_active address (0x8008e17c), which with
 * WLDCORE loaded lies inside travel_path_find; WORLD's copy of the battle menus is dead code (QUIRKS.md). */
void camera_idle_wait(void) {
    do {
        thread_yield();
    } while (camera_is_active() != 0);
}

/* Reduce the yaw script variable to one turn [0, ONE). Adding ten whole turns first keeps
 * yaws down to -10 turns positive before the mask. */
void camera_yaw_normalize(void) {
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]
        = (g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW] + 0xa000) & 0x0fff;
}

/* Step the camera tilt by 0x10 per frame to the nearer of the two resting tilts, 0x12e
 * (low) or 0x1c0 (high), split at 0x177; then point the map's tilt target at the same
 * resting tilt, zoom the map in and restore the zoom variable to ONE. */
void camera_tilt_zoom_init(void) {
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
s32 camera_yaw_wrap(void) {
    s32 yaw = g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW];

    while (yaw < 0) {
        yaw += ONE;
    }
    return yaw;
}

/* Return the stats for the menu's selected battle slot. */
unit_stats_t* menu_selected_unit_stats_get(void) {
    return unit_stats_get(g_unit_panel_selected_unit.battle_id);
}

/* Clamp a signed value, testing the lower bound first. */
void script_value_clamp(s32* value, s32 min, s32 max) {
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
 * the secondary period. The initial delay and both repeat periods are divided
 * by the event speed.
 *
 * No input at all, or a tutorial event that has Up or Down disabled, resets the
 * three repeat counters. WORLD handles its tutorial text input separately in
 * script_route_event_input, which fetches the buttons itself. */
void script_event_input_update(u32 input) {
    u32 filtered_input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_repeat_delay;
    u32 previous_input;
    u32 counter;

    if (input == 0
        || (tutorial_is_current_event() != 0
            && (script_variable_get(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) & (PSX_PAD_UP | PSX_PAD_DOWN)) != 0)) {
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
            secondary_repeat_delay = g_input_secondary_repeat_period / g_script_event_speed;
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
