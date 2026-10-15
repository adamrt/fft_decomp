#include "fft/unit.h"
#include "psx/types.h"

/* The battle stats of an occupied unit slot, or 0 for an empty slot or an id past
 * the last slot. */
unit_stats_t* battle_unit_get_existing_pointer(u32 battle_id) {
    unit_stats_t* unit = 0;
    if (battle_id < BATTLE_UNIT_SLOT_COUNT) {
        unit = &g_battle_unit_stats[battle_id];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            unit = 0;
        }
    }
    return unit;
}
