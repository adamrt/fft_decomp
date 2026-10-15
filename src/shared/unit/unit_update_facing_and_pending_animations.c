#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

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
