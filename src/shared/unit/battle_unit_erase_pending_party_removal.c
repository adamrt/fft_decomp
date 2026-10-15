#include "fft/battle_unit.h"
#include "psx/types.h"

/* Empties a unit slot and sets its existence to pending removal, without testing
 * whether the unit is present. No retail code calls it. */
s32 battle_unit_erase_pending_party_removal(s32 unit_id) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
    unit->existence = BATTLE_UNIT_EXISTENCE_PENDING_REMOVAL;
    return 0;
}
