#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Mark the unit's move and/or action as taken and end its turn once nothing is
 * left to do. -1 for an absent unit, 1 when the turn ended, otherwise 0. */
s32 action_set_move_act_flags(s32 idx, s32 move_flag, s32 act_flag) {
    unit_stats_t* unit;
    s32 ret;
    unit = &g_battle_unit_stats[idx];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return -1;
    if (move_flag != 0) {
        unit->movement_taken = 1;
    }
    if (act_flag != 0) {
        unit->action_taken = 1;
    }
    ret = action_should_end_unit_turn(unit);
    if (ret != 0) {
        ret = action_end_turn(idx);
    }
    return ret;
}
