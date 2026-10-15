#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* 1 when the teleport succeeds: always with Teleport 2, otherwise it fails
 * with a 10 in 100 chance per tile of distance beyond Move. */
s32 move_calculate_teleport_chances(void) {
    move_pathfind_scratch_t* ctx = g_move_config_ptr;
    s32 dx, dy;

    if (ctx->movement_set_2 & BATTLE_MOVEMENT_SET_2_TELEPORT_2) {
        return 1;
    }
    dy = ctx->target_x - ctx->x;
    dx = ctx->target_y - ctx->y;
    if (dy < 0)
        dy = -dy;
    if (dx < 0)
        dx = -dx;
    return main_util_roll_pass_fail(0x64, ((dy + dx) - ctx->move) * 10);
}
