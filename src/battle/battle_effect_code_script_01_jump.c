#include "fft/battle.h"

s32 battle_effect_code_script_01_jump(effect_record_t* effect) {
    s32 pc = effect->pc;
    s32 base = (s32)effect->script;
    effect->pc = *(u16*)(pc + base + 2);
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
