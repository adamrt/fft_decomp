#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Advances one particle channel of the parent timeline by a frame: enters the
 * keyframes that have come due, running each one's 16-bit action value for every
 * effect target, then spawns the current keyframe's emitter, through the
 * handler of the action's callback slot when it names one. The handler is
 * told to destroy its work on the call made with two frames of the keyframe
 * left. index, remaining and step are the channel's keyframe, the frames left
 * in it and the frames it has run. */
void effect_step_emitter_timeline(
    effect_record_t* state, effect_keyframe_table_t* table, s16* index, s16* remaining, s16* step) {
    s16 k;
    s32 sel;
    u16* actions;
    u16 slot;
    s32 emitter;

    if (table->last_index >= *index) {
        while (*remaining == 0) {
            k = *index;
            *index = k + 1;
            /* Integer-first pointer addition preserves the native shared-address operands. */
            effect_run_keyframe_actions_all_targets(*(k + 1 + table->action), state);
            *remaining = table->frame_start[k + 1] - table->frame_start[k];
            *step = 0;
            if (table->last_index < *index) {
                return;
            }
        }
        if (table->last_index >= *index) {
            emitter = table->emitter[*index - 1];
            if (emitter != 0) {
                actions = table->action;
                sel = actions[*index] & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK;
                slot = sel - 1;
                if (sel != 0) {
                    if (*remaining == 2) {
                        state->callback_phase[slot] = EFFECT_CALLBACK_PHASE_DESTROY;
                    }
                    state->callbacks[slot](state->record_index, slot, emitter - 1, *step);
                } else {
                    effect_spawn_emitter_particles(state->record_index, *step, emitter - 1, 0);
                }
                *step = *step + 1;
            }
            *remaining = *remaining - 1;
        }
    }
}
