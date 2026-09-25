#include "fft/battle.h"

/* Jump to the offset in the third halfword when the counter selected by the
 * second byte's top two bits differs from the second halfword; otherwise step
 * over the 6-byte instruction. */
s32 battle_effect_code_script_1c_jump_if_counter_not_equal(effect_record_t* effect) {
    u8* cur;
    u32 slot;
    s16 pc;

    pc = effect->pc;
    cur = (u8*)(pc + (s32)effect->script);
    slot = (u32)cur[1] >> 6;
    if (effect->counters[slot] != *(s16*)(cur + 2)) {
        effect->pc = *(u16*)(cur + 4);
    } else {
        effect->pc = pc + 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
