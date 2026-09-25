#include "fft/battle.h"

s32 battle_effect_code_script_1a_jump_if_child_active(effect_record_t* ctx) {
    u8* cur;
    u32 slot;

    cur = (u8*)(ctx->pc + (s32)ctx->script);
    slot = (u32)cur[1] >> 4;
    if (ctx->phase_ids[slot] != 0) {
        ctx->pc = *(u16*)(cur + 2);
    } else {
        ctx->pc = ctx->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
