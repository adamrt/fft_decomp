#include "fft/battle.h"

/* Clear the child-timeline state (target, frame and keyframe tracks) that
 * opcode 28 steps. */
s32 battle_effect_code_script_2a_reset_child_timeline(effect_record_t* effect) {
    battle_effect_reset_child_timeline((battle_effect_temp_data_t*)effect, 0);
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
