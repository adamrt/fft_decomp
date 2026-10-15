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

/* Falls off the end: move_calculate_pathing leaves the path block pointer in $v0 and
 * the caller consumes it, so the result type belongs on this definition. */
battle_walk_path_t* move_calculate_walkto_pathing(
    s32 flags, s32 jump, s32 x, s32 y, s32 level, s32 target_x, s32 target_y, s32 target_level) {
    s32 suspended;
    move_calculate_pathing(flags, jump, x, y, level, target_x, target_y, target_level, 1, &suspended, 0);
}
