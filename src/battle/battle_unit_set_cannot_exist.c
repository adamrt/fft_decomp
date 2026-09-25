#include "fft/battle.h"
#include "psx/types.h"

s32 battle_unit_set_cannot_exist(s32 unit_index) {
    battle_stats_t* unit;
    unit = &g_battle_unit_stats[unit_index];
    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
    unit->existence = 0;
    return 0;
}
