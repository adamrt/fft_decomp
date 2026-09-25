#include "fft/battle.h"
#include "fft/battle_move.h"
#include "fft/battle_unit_rotation.h"
#include "fft/unit_slots.h"
#include "fft/world.h"
#include "psx/types.h"

/* Battle twin of world_unit_update_facing_and_pending_animations: per frame,
 * apply pending animations to units that stopped moving and step each rotating
 * unit one facing toward its target. */
void battle_unit_update_facing_and_pending_animations(void) {
    s32 i;
    s32 facing;
    s32 current;
    s32 next;
    s32 moving;
    battle_unit_rotation_state_t* state;

    state = g_battle_unit_misc_rotation_data;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++, state++) {
        if (battle_unit_has_misc_id(i) != 0) {
            /* Both tables are read unsigned (lhu) here. */
            if (((u16*)g_battle_unit_pending_animation_ids)[i] != 0) {
                moving = battle_move_is_unit_moving_by_misc_id(i);
                if (moving == 0 && ((u16*)g_battle_unit_was_moving_latch)[i] != 0) {
                    battle_unit_set_specific_animation_value_on_battle_init(
                        i, ((u16*)g_battle_unit_pending_animation_ids)[i]);
                    g_battle_unit_pending_animation_ids[i] = 0;
                }
                g_battle_unit_was_moving_latch[i] = moving;
            }
            current = battle_unit_get_facing_nibble_by_misc_id(i);
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
            if (state->rotate_mode == 0) {
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
            } else if (state->rotate_mode == 1) {
                facing = (current + 1) & 0xF;
            } else if (state->rotate_mode == 2) {
                facing = (current + 0xF) & 0xF;
            }
            battle_unit_init_for_store_anim_facing_move(i, facing);
        } else {
            g_battle_unit_was_moving_latch[i] = 0;
            g_battle_unit_pending_animation_ids[i] = 0;
        }
    }
}
