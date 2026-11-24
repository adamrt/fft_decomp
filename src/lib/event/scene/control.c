/* Event unit commands, screen and camera transitions, menu sounds and event input. */

#include "fft/battle/camera.h"
#include "fft/game/battle_unit.h"
#include "fft/game/status.h"
#include "fft/gfx/animation.h"
#include "fft/gfx/fade.h"
#include "fft/map/map.h"
#include "fft/map/map_unit.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_list.h"
#include "fft/party/unit_panel.h"
#include "fft/script/script.h"
#include "fft/script/script_instruction.h"
#include "fft/script/script_variable.h"
#include "fft/sys/input.h"
#include "fft/sys/sound.h"
#include "fft/sys/thread.h"
#include "fft/world/tutorial.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* As an event starts in battle, run each of its InflictStatus instructions whose unit
 * has Float or Jump, one at a time on a free thread. */
void script_process_status_commands(void) {
    s32 instruction_offset = 0;
    s32 map_unit_id;
    s32 battle_unit_id;
    s32 thread_id;
    const u8* instruction;
    battle_unit_t* unit;

    while (
        (instruction_offset = script_find_instruction_offset(instruction_offset, SCRIPT_OPCODE_INFLICT_STATUS)) != 0) {
        instruction = (const u8*)g_script_event_block;
        if (instruction[instruction_offset] == SCRIPT_OPCODE_INFLICT_STATUS) {
            map_unit_id = script_resolve_map_unit_id(script_read_s16(instruction + instruction_offset + 1));
            if (map_unit_id != SCRIPT_MAP_UNIT_ID_NONE) {
                battle_unit_id = map_unit_get_battle_unit_id(map_unit_id);
                /* SCRIPT_BATTLE_UNIT_ID_NONE is battle_unit_get_id_by_event_unit_id's "no unit" value;
                 * this lookup fails with -1 instead, so the test never rejects anything. */
                if (battle_unit_id != SCRIPT_BATTLE_UNIT_ID_NONE) {
                    unit = battle_unit_get(battle_unit_id);
                    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
                            & STATUS_BYTE_MASK(STATUS_ID_FLOAT))
                        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
                            & STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                        thread_id = thread_resolve_slot(THREAD_SLOT_FIRST_IDLE);
                        thread_start(thread_id, script_inflict_status_thread);
                        thread_set_parameters(
                            thread_id, (s32)((const u8*)g_script_event_block + instruction_offset + 1), 0, 0);
                        thread_wait_inactive(thread_id);
                    }
                }
            }
        }
        instruction_offset += g_script_event_instruction_sizes[SCRIPT_OPCODE_INFLICT_STATUS] + 1;
    }
}

/* Set a unit's animation. Once the battle has started and script variable
 * SCRIPT_VAR_SHOP_ITEM_AVAILABILITY is set, a unit that rides or carries
 * another gets its status animation instead, and a unit standing in water of depth 2
 * or more uses the standing pose selector; both run on the main stack. Animations from
 * ANIMATION_SEQUENCE_EVTCHR_SLOT_0 up are always played as requested. */
void map_unit_set_event_animation(s32 map_unit_id, u32 animation) {
    s32 battle_unit_id;
    battle_unit_t* battle_unit;
    s32 depth;

    if (script_variable_get(SCRIPT_VAR_BATTLE_NOT_STARTED) == 0
        && script_variable_get(SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) != 0) {
        battle_unit_id = map_unit_get_battle_unit_id(map_unit_id);
        if (battle_unit_id >= 0 && animation < ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
            battle_unit = battle_unit_get(battle_unit_id);
            depth = map_get_tile_value(
                MAP_TILE_DATA_DEPTH, battle_unit->x, battle_unit->position.y, battle_unit->position.higher_elevation);
            if ((battle_unit->mount_info & UNIT_MOUNT_INFO_ROLE_MASK) != 0) {
                g_thread_call_target = (void (*)(void))animation_map_unit_set_status;
            } else if ((depth & 0xFF) >= 2) {
                g_thread_call_target = (void (*)(void))animation_map_unit_set_stand;
            } else {
                animation_map_unit_request(map_unit_id, animation);
                return;
            }
            thread_call_on_main_stack(map_unit_id);
            return;
        }
    }
    animation_map_unit_request(map_unit_id, animation);
}

/* Faces a unit, and its mount partner when one is linked, in a direction given in
 * sixteenths of a turn. Script variable SCRIPT_VAR_BATTLE_NOT_STARTED
 * suppresses the partner's update. */
void map_unit_set_event_facing(s32 map_unit_id, s32 facing) {
    s32 battle_unit_id;
    battle_unit_t* battle_unit;

    if (script_variable_get(SCRIPT_VAR_BATTLE_NOT_STARTED) == 0) {
        battle_unit_id = map_unit_get_battle_unit_id(map_unit_id);
        if (battle_unit_id >= 0) {
            battle_unit = battle_unit_get(battle_unit_id);
            if (battle_unit->mount_info != 0) {
                map_unit_set_facing_sixteenth(
                    map_unit_get_id_by_battle_unit_id(battle_unit->mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK),
                    facing & 0xFFFF);
            }
        }
    }
    map_unit_set_facing_sixteenth(map_unit_id, facing & 0xFFFF);
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

/* Fade the screen in: lower g_fade_event_level from 255 by 4 per frame, yielding once per step, and
 * end on 0. */
void fade_event_run_in(void) {
    g_fade_event_level = 0xFF;
    do {
        thread_yield();
        g_fade_event_level -= 4;
    } while (g_fade_event_level > 0);
    g_fade_event_level = 0;
}

/* Fade the screen out: raise g_fade_event_level from 0 by 4 per frame,
 * yielding once per step, and end on 255. */
void fade_event_run_out(void) {
    g_fade_event_level = 0;
    do {
        thread_yield();
        g_fade_event_level += 4;
    } while (g_fade_event_level < 0x100);
    g_fade_event_level = 0xff;
}

/* 1 while the current event id is one of the sixteen tutorial events,
 * 0x19a..0x1a9; WORLD has no tutorial events and always returns 0. */
s32 tutorial_is_current_event(void) {
#ifdef FFT_BATTLE
    s32 event_id = script_variable_get(SCRIPT_VAR_CURRENT_EVENT);

    return event_id >= EVENT_ID_TUTORIAL_FIRST && event_id < EVENT_ID_TUTORIAL_FIRST + 16;
#else
    return 0;
#endif
}

/* The battle unit id of the unit an event unit id resolves to, or SCRIPT_BATTLE_UNIT_ID_NONE (2000) when it
 * resolves to no map unit or one without battle stats. */
s32 battle_unit_get_id_by_event_unit_id(s32 event_unit_id) {
    s32 map_unit_id = script_resolve_map_unit_id(event_unit_id);

    if (map_unit_id != SCRIPT_MAP_UNIT_ID_NONE) {
        s32 battle_unit_id = map_unit_get_battle_unit_id(map_unit_id);

        if (battle_unit_id != -1) {
            return battle_unit_id;
        }
    }
    return SCRIPT_BATTLE_UNIT_ID_NONE;
}

/* Set the flag that menu_mark_modal_closed clears: the companion overlay openers (OPTION.OUT and
 * the others) call it before loading their executable. */
void menu_mark_modal_open(void) {
    g_menu_modal_open = 1;
}

/* Clears the "an options/menu overlay is on screen" flag. Called from the
 * BATTLE.BIN overlay loaders (EQUIP.OUT, DEBUGCHR.OUT, ...) once the overlay
 * has been torn down. */
void menu_mark_modal_closed(void) {
    g_menu_modal_open = 0;
}

/* Queue the confirm sound; the frame update plays the pending effect. */
void sound_queue_confirm(void) {
    g_sound_pending_sfx_id = SOUND_SFX_CONFIRM;
}

/* Queue the cancel sound; the frame update plays the pending effect. */
void sound_queue_cancel(void) {
    g_sound_pending_sfx_id = SOUND_SFX_CANCEL;
}

/* Queue the cursor-move sound; the frame update plays the pending effect. */
void sound_queue_cursor(void) {
    g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
}

/* Queue the invalid-choice sound; the frame update plays the pending effect. */
void sound_queue_invalid(void) {
    g_sound_pending_sfx_id = SOUND_SFX_INVALID;
}

/* Menu thread for a refused command: queue the invalid-choice sound and exit. */
void menu_invalid_choice_thread(void) {
    sound_queue_invalid();
    thread_current_exit();
}

/* Yields the thread at least once, then until the camera stops moving. WORLD's
 * copy calls BATTLE's camera_is_active address (0x8008e17c), which with
 * WLDCORE loaded lies inside find_path; WORLD's copy of the battle menus is dead code (QUIRKS.md). */
void camera_wait_idle(void) {
    do {
        thread_yield();
    } while (camera_is_active() != 0);
}

/* Reduce the yaw script variable to one turn [0, ONE). Adding ten whole turns first keeps
 * yaws down to -10 turns positive before the mask. */
void camera_normalize_yaw(void) {
    g_script_variables[SCRIPT_VAR_CAMERA_YAW] = (g_script_variables[SCRIPT_VAR_CAMERA_YAW] + 10 * ONE) & (ONE - 1);
}

/* Step the camera tilt by 0x10 per frame to the nearer of the two resting tilts, 0x12e
 * (low) or 0x1c0 (high), split at 0x177; then point the map's tilt target at the same
 * resting tilt, zoom the map in and restore the zoom variable to ONE. */
void camera_settle_tilt_zoom(void) {
    s32 current_tilt = g_script_variables[SCRIPT_VAR_CAMERA_TILT];
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
            g_script_variables[SCRIPT_VAR_CAMERA_TILT] = current_tilt;
            thread_yield();
            current_tilt += tilt_step;
        }
        g_script_variables[SCRIPT_VAR_CAMERA_TILT] = target_tilt;
    } else {
        while (current_tilt < target_tilt) {
            g_script_variables[SCRIPT_VAR_CAMERA_TILT] = current_tilt;
            thread_yield();
            current_tilt += tilt_step;
        }
        g_script_variables[SCRIPT_VAR_CAMERA_TILT] = target_tilt;
    }

    g_camera_tilt_target = tilt_target_mode + 1;
    g_camera_zoom_target = CAMERA_ZOOM_IN;
    g_script_variables[SCRIPT_VAR_CAMERA_ZOOM] = ONE;
}

/* Raise negative yaw by whole turns; retain positive values of ONE or greater. */
s32 camera_get_nonnegative_yaw(void) {
    s32 yaw = g_script_variables[SCRIPT_VAR_CAMERA_YAW];

    while (yaw < 0) {
        yaw += ONE;
    }
    return yaw;
}

/* Return the stats for the menu's selected battle slot. */
battle_unit_t* unit_panel_get_selected_battle_unit(void) {
    return battle_unit_get(g_unit_panel_selected_unit.battle_unit_id);
}

/* Clamp a signed value, testing the lower bound first. */
void script_clamp_value(s32* value, s32 min, s32 max) {
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
void script_update_event_input(u32 input) {
    u32 filtered_input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_repeat_delay;
    u32 previous_input;
    u32 counter;

    if (input == 0
        || (tutorial_is_current_event() != 0
            && (script_variable_get(SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) & (PSX_PAD_UP | PSX_PAD_DOWN)) != 0)) {
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
