/* Animation state reset, pending animations, event thread start/reset and deployment test. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/types.h"

void unit_reset_animation_states(void) {
    unit_animation_state_t* state;
    s32 i;

    i = 0;
    state = g_unit_animation_states;
    for (; i < BATTLE_UNIT_SLOT_COUNT; i++, state++) {
        state->rotating = 0;
        state->delay = 0;
        if (battle_unit_has_runtime_id(i) != 0) {
            state->initial_facing = battle_unit_get_facing_nibble_by_runtime_id(i);
        } else {
            state->initial_facing = 0xff;
        }
        g_unit_pending_animation_ids[i] = 0;
        g_unit_was_moving_latch[i] = 0;
    }
}

/* Per frame, apply each unit's pending animation on the frame it stops moving
 * and step each rotating unit one facing (of 16) toward its target, after its
 * delay and once every frames_per_step frames. The shortest mode turns
 * whichever way is nearer; the other two modes always increment or decrement.
 * Nothing calls it in WORLD, where its caller script_process_pending_requests_variant
 * is empty. */
void unit_update_facing_and_pending_animations(void) {
    s32 i;
    s32 facing;
    s32 current;
    s32 next;
    s32 moving;
    unit_animation_state_t* state;

    state = g_unit_animation_states;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++, state++) {
        if (battle_unit_has_runtime_id(i) != 0) {
            if (g_unit_pending_animation_ids[i] != 0) {
                moving = move_is_unit_moving_by_runtime_id(i);
                if (moving == 0 && g_unit_was_moving_latch[i] != 0) {
                    unit_set_animation(i, g_unit_pending_animation_ids[i]);
                    g_unit_pending_animation_ids[i] = 0;
                }
                g_unit_was_moving_latch[i] = moving;
            }
            current = battle_unit_get_facing_nibble_by_runtime_id(i);
            if (state->rotating == 0) {
                continue;
            }
            if (current == state->target_facing) {
                state->rotating = 0;
                continue;
            }
            state->step_counter++;
            if (state->delay != 0) {
                state->delay--;
                continue;
            }
            if (state->step_counter < state->frames_per_step) {
                continue;
            }
            state->step_counter = 0;
            if (state->rotate_mode == UNIT_ROTATE_MODE_SHORTEST) {
                moving = state->target_facing - current + 0x10;
                facing = current - state->target_facing + 0x10;
                moving &= 0xF;
                facing &= 0xF;
                if (facing < moving) {
                    next = current + 0xF;
                } else {
                    next = current + 1;
                }
                facing = next & 0xF;
            } else if (state->rotate_mode == UNIT_ROTATE_MODE_INCREMENT) {
                facing = (current + 1) & 0xF;
            } else if (state->rotate_mode == UNIT_ROTATE_MODE_DECREMENT) {
                facing = (current + 0xF) & 0xF;
            }
            unit_set_facing(i, facing);
        } else {
            g_unit_was_moving_latch[i] = 0;
            g_unit_pending_animation_ids[i] = 0;
        }
    }
}

/* Reset the unit animation states and, when the current-event variable is set,
 * fade the screen color modulation to zero and start the event thread (slot 1)
 * on script_run_next_event.
 *
 * While the battle has not started it first stores the camera in the script
 * variables and resets the script camera transform. Where WORLD's copy calls two
 * empty functions, BATTLE does that camera work. */
void script_start_current_event_thread(void) {
    unit_reset_animation_states();
    g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_NORMAL;
    if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
        camera_store_state_to_script_variables_variant();
        map_reset_script_light_transform_variant();
        g_camera_position_locked = 0;
        g_camera_speed_curve = 0;
    }
    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0) {
        g_camera_position_locked = 0;
        g_camera_speed_curve = 0;
        battle_gfx_start_screen_color_modulation_fade(2, 0, 0, 0, 2);
        thread_start(THREAD_SLOT_EVENT, script_run_next_event);
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * script_start_current_event_thread and script_reset_event_state. Nothing on
 * the disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void script_noop_8(void) {
}

/* Reset unit animation state; while the event slot is idle, run the scene
 * camera/light reset hooks and clear camera interpolation and position lock. */
void script_reset_event_state(void) {
    unit_reset_animation_states();
    if (thread_is_running_asm_variant(THREAD_SLOT_EVENT) == 0) {
        camera_store_state_to_script_variables_variant();
        map_reset_script_light_transform_variant();
        g_camera_speed_curve = 0;
        g_camera_position_locked = 0;
    }
}

/* Run one event frame, then return whether the deployment thread (borrowed slot
 * 6) is still running. */
s32 script_is_deployment_running(u32* ot, u32 buttons) {
    script_run_event_frame_variant(ot, buttons);
    return thread_is_running(THREAD_BORROWED_SLOT_DEPLOYMENT);
}
