#include "fft/battle.h"
#include "psx/types.h"

s32 battle_unit_check_exists(s32 unit_id) {
    battle_stats_t* unit;

    unit = &g_battle_unit_stats[unit_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return -1;
    if (unit->formation_index == 0xFF)
        return -1;
    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
    unit->existence = BATTLE_UNIT_EXISTENCE_PENDING_REMOVAL;
    return 0;
}
