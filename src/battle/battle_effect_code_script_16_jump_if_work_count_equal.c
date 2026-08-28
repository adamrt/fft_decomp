#include "fft/effect.h"

/* Jump to the offset in the third halfword when the record work count equals
 * the second halfword; otherwise step over the 6-byte instruction. */
s32 battle_effect_code_script_16_jump_if_work_count_equal(effect_record_t* effect) {
    s16 pc = effect->pc;
    s16* instruction = (s16*)(pc + (s32)effect->script);
    if ((s16)effect->work_count == instruction[1]) {
        effect->pc = (s16) * (u16*)(instruction + 2);
    } else {
        effect->pc = pc + 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
