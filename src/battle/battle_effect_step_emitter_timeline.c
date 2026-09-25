/* Two commutative operand ties decide this function, in opposite directions.
 * The dispatch block at +0xe4 wants its table address built base-first, which
 * hoisting the selector array into its own pointer provides.  The keyframe
 * loop at +0x68 wants the shared `table + 2*k` address built index-first
 * (`addu s0,s0,s2`).  gcc 2.6.3 keeps the source's operand order through to
 * the addu, so `p[i]` and `*(p + i)` build base-first while the int-first
 * sums `*(i + p)` and `i[p]` build index-first -- see the comment at that
 * call.
 */
#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_step_emitter_timeline(battle_keyframe_effect_state_t* state, battle_effect_keyframe_table_t* table,
    s16* index, s16* remaining, s16* step) {
    s16 k;
    s32 sel;
    u16* selectors;
    u16 slot;
    s32 action;

    if (table->last_index >= *index) {
        while (*remaining == 0) {
            k = *index;
            *index = k + 1;
            /* The int-first sum is load-bearing: the shared address
             * `table + 2*k` is CSEd from this first use, and only this
             * spelling builds it index-first (`addu s0,s0,s2`) as the target
             * does.  `table->selector[k + 1]` builds it base-first. */
            battle_effect_run_keyframe_actions_all_targets(*(k + 1 + table->selector), state);
            *remaining = table->frame_start[k + 1] - table->frame_start[k];
            *step = 0;
            if (table->last_index < *index) {
                return;
            }
        }
        if (table->last_index >= *index) {
            action = table->action[*index];
            if (action != 0) {
                selectors = table->selector;
                sel = selectors[*index] & 7;
                slot = sel - 1;
                if (sel != 0) {
                    if (*remaining == 2) {
                        state->callback_state[slot] = 3;
                    }
                    ((void (*)(s16, s32, s32, s16))state->callback_ptrs[slot])(
                        state->callback_arg_02, slot, action - 1, *step);
                } else {
                    battle_effect_spawn_emitter_particles(state->callback_arg_02, *step, action - 1, 0);
                }
                *step = *step + 1;
            }
            *remaining = *remaining - 1;
        }
    }
}
