#include "fft/effect.h"

s32 battle_effect_code_script_19_jump_if_child_count_equal(effect_record_t* effect) {
    s16 i;
    s32 count;
    u16 target;

    target = *(u16*)(effect->pc + (s32)effect->script + 2);
    count = 0;
    for (i = 0; i < 4; i++) {
        if (effect->phase_ids[i] != 0) {
            count++;
        }
    }
    if ((s16)count == (s16)target) {
        effect->pc = *(u16*)(effect->pc + (s32)effect->script + 4);
    } else {
        effect->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
