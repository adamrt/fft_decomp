#include "fft/effect.h"

struct temp_effect_data_2_t;
extern void battle_effect_reset_parent_timeline(struct temp_effect_data_2_t* data);

/* Clear the parent-timeline state (frame, spawn counters and keyframe tracks)
 * that opcode 29 steps. */
s32 battle_effect_code_script_2b_reset_parent_timeline(effect_record_t* effect) {
    battle_effect_reset_parent_timeline((struct temp_effect_data_2_t*)effect);
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
