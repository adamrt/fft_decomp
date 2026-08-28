#include "fft/battle.h"
#include "psx/types.h"

/* battle_move_calculate_pathing with the per-call search budget enabled, so a
 * long search can suspend and resume through `suspended`. */
void battle_move_calculate_pathing_time_sliced(s32 flags, s32 jump, s32 x, s32 y, s32 level, s32 target_x, s32 target_y,
    s32 target_level, s32 initialize, s32 suspended) {
    battle_move_calculate_pathing(
        flags, jump, x, y, level, target_x, target_y, target_level, initialize, (s32*)suspended, 1);
}
