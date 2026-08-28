#include "fft/battle.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

battle_stats_t* battle_unit_get_existing_pointer(u32 unit_id) {
    battle_stats_t* unit = 0;
    if (unit_id < BATTLE_UNIT_SLOT_COUNT) {
        unit = &g_battle_unit_stats[unit_id];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            unit = 0;
        }
    }
    return unit;
}
