#include "fft/battle.h"

s32 battle_effect_code_script_20_increment_selected_counter(effect_record_t* effect) {
    u8* instruction;

    instruction = effect->script + effect->pc;
    effect->counters[instruction[1] >> 6] += 1;
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
