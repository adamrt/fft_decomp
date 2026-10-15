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

/* move_calculate_pathing with the per-call search budget enabled, so a
 * long search can suspend and resume through `suspended`. */
void move_calculate_pathing_time_sliced(s32 flags, s32 jump, s32 x, s32 y, s32 level, s32 target_x, s32 target_y,
    s32 target_level, s32 initialize, s32* suspended) {
    move_calculate_pathing(flags, jump, x, y, level, target_x, target_y, target_level, initialize, suspended, 1);
}
