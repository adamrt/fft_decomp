#include "fft/unit.h"
#include "psx/types.h"

/* The battle stats in a unit slot, occupied or not; 0 for an battle_id past the last
 * slot. */
unit_stats_t* battle_unit_get_stats_from_battle_id(u32 battle_id) {
    if (battle_id >= BATTLE_UNIT_SLOT_COUNT) {
        return 0;
    }
    return &g_battle_unit_stats[battle_id];
}
