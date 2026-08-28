#include "fft/effect.h"

s32 battle_effect_code_script_2c_nop(effect_record_t* effect) {
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
