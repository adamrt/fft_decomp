#include "fft/battle.h"
#include "fft/effect.h"

s32 battle_effect_code_script_05(effect_record_t* effect) {
    u8* instruction = effect->script + effect->pc;

    D_801BF000 = instruction[1] >> 4;
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
