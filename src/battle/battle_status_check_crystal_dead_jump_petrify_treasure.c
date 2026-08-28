#include "fft/battle.h"

s32 battle_status_check_crystal_dead_jump_petrify_treasure(battle_stats_t* unit) {
    s32 result;
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return 1;
    if (unit->status_sets.current[0]
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD)
            | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP)))
        return 1;
    result = unit->status_sets.current[1]
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PETRIFY) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE));
    if (result != 0)
        result = 1;
    return result;
}
