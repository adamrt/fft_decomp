#include "fft/battle.h"
#include "psx/types.h"

s32 battle_unit_disable_remove(s32 unit_id) {
    battle_stats_t* unit = &g_battle_unit_stats[unit_id];
    if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
        unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
        unit->existence = BATTLE_UNIT_EXISTENCE_DISABLED;
    }
    return 0;
}
