#include "fft/effect.h"

s32 battle_effect_code_script_23_subtract_immediate_from_selected_counter(effect_record_t* effect) {
    u8* instruction;
    u16 amount;

    instruction = effect->script + effect->pc;
    amount = *(u16*)(instruction + 2);
    effect->counters[instruction[1] >> 6] -= amount;
    effect->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
