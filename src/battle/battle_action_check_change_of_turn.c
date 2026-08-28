#include "fft/battle.h"
#include "psx/types.h"

s32 battle_action_check_change_of_turn(s32 unit_id) {
    battle_stats_t* unit = &g_battle_unit_stats[unit_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 1;
    }
    if (!(battle_status_check_unit(unit) & BATTLE_TURN_STATUS_BLOCKS_TURN_MASK)) {
        return unit->has_turn == 0;
    }
    unit->has_turn = 0;
    return 1;
}
