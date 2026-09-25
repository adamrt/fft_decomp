#include "fft/battle.h"

s32 battle_effect_code_script_13_jump_if_counter_greater(effect_record_t* ctx) {
    s16 pc = ctx->pc;
    u8* cur = (u8*)(pc + (s32)ctx->script);
    s16* counter = &ctx->counters[(u32)cur[1] >> 6];
    if (*counter > *(s16*)(cur + 2)) {
        ctx->pc = (s16) * (u16*)(cur + 4);
    } else {
        ctx->pc = pc + 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
