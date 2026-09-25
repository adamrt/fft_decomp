#include "fft/battle.h"

s32 battle_effect_code_script_10_set_selected_counter(effect_record_t* ctx) {
    u8* cur;
    u32 slot;

    cur = (u8*)(ctx->pc + (s32)ctx->script);
    slot = (u32)cur[1] >> 6;
    ctx->counters[slot] = *(u16*)(cur + 2);
    ctx->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
