#include "fft/battle.h"
#include "psx/types.h"

s32 battle_action_set_move_act_flags(s32 idx, s32 move_flag, s32 act_flag) {
    battle_stats_t* unit;
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
    ret = battle_action_should_end_unit_turn(unit);
    if (ret != 0) {
        ret = battle_action_end_turn(idx);
    }
    return ret;
}
