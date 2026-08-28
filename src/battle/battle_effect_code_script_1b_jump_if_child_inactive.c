#include "fft/effect.h"

/* Jump to the offset in the second halfword when the phase child selected by
 * the second byte's high nibble is 0; otherwise step over the 4-byte
 * instruction. */
s32 battle_effect_code_script_1b_jump_if_child_inactive(effect_record_t* effect) {
    u8* cur;
    u32 slot;

    cur = (u8*)(effect->pc + (s32)effect->script);
    slot = (u32)cur[1] >> 4;
    if (effect->phase_ids[slot] == 0) {
        effect->pc = *(u16*)(cur + 2);
    } else {
        effect->pc = effect->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
