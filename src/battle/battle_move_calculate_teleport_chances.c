#include "fft/battle_move.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

s32 battle_move_calculate_teleport_chances(void) {
    battle_move_pathfind_scratch_t* ctx = g_battle_move_config_ptr;
    s32 dx, dy;

    if (ctx->movement_set_2 & 4) {
        return 1;
    }
    dy = ctx->target_x - ctx->x;
    dx = ctx->target_y - ctx->y;
    if (dy < 0)
        dy = -dy;
    if (dx < 0)
        dx = -dx;
    /* The target passes ctx as a third argument the two-parameter callee ignores. */
    return ((s32 (*)(s32, s32, void*))main_util_roll_pass_fail)(0x64, ((dy + dx) - ctx->move) * 10, ctx);
}

/* padding */
