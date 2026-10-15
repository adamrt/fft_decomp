#include "fft/battle_unit.h"
#include "psx/types.h"

/* Empties a unit slot and marks it off the field (defined but not placed). Always
 * returns 0. */
s32 battle_unit_set_cannot_exist(s32 battle_id) {
    unit_stats_t* unit;
    unit = &g_battle_unit_stats[battle_id];
    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
    unit->existence = BATTLE_UNIT_EXISTENCE_OFF_FIELD;
    return 0;
}
