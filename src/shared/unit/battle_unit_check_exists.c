#include "fft/battle_unit.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Marks a present unit that has a formation index for removal: the slot is
 * emptied and its existence set to pending removal. Returns -1 when the slot is
 * empty or has no formation index, else 0. No retail code calls it. */
s32 battle_unit_check_exists(s32 unit_id) {
    unit_stats_t* unit;

    unit = &g_battle_unit_stats[unit_id];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return -1;
    if (unit->formation_index == BATTLE_FORMATION_INDEX_NONE)
        return -1;
    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
    unit->existence = BATTLE_UNIT_EXISTENCE_PENDING_REMOVAL;
    return 0;
}
