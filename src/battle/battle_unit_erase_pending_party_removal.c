#include "fft/battle.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

s32 battle_unit_erase_pending_party_removal(s32 unit_id) {
    battle_stats_t* unit = &g_battle_unit_stats[unit_id];
    unit->entd_slot = BATTLE_ENTD_SLOT_NONE;
    unit->existence = BATTLE_UNIT_EXISTENCE_PENDING_REMOVAL;
    return 0;
}
