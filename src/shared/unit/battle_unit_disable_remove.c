#include "fft/battle_unit.h"
#include "psx/types.h"

/* Takes a present unit out of the battle: the slot is emptied and its existence
 * set to BATTLE_UNIT_EXISTENCE_DISABLED. Always returns 0. */
s32 battle_unit_disable_remove(s32 battle_id) {
    unit_stats_t* unit = &g_battle_unit_stats[battle_id];
    if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
        unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
        unit->existence = BATTLE_UNIT_EXISTENCE_DISABLED;
    }
    return 0;
}
