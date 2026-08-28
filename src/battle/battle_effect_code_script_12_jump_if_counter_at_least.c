#include "fft/effect.h"

/* Jump to the offset in the third halfword when the counter selected by the
 * top two bits of the second byte is at least the second halfword; otherwise
 * step over the 6-byte instruction. */
s32 battle_effect_code_script_12_jump_if_counter_at_least(effect_record_t* effect) {
    s16 pc = effect->pc;
    u8* instruction = (u8*)(pc + (s32)effect->script);
    if (effect->counters[instruction[1] >> 6] >= *(s16*)(instruction + 2)) {
        effect->pc = (s16) * (u16*)(instruction + 4);
    } else {
        effect->pc = pc + 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
