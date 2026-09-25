#include "fft/effect.h"

/* Jump to the offset in the third halfword when the counter selected by the
 * top two bits of the second byte is below the second halfword; otherwise
 * step over the 6-byte instruction. */
s32 battle_effect_code_script_15_jump_if_counter_less(effect_record_t* effect) {
    s16 pc = effect->pc;
    u8* instruction = effect->script + pc;
    if (effect->counters[instruction[1] >> 6] < *(s16*)(instruction + 2)) {
        effect->pc = (s16) * (u16*)(instruction + 4);
    } else {
        effect->pc = pc + 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
