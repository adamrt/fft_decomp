#include "fft/battle_unit.h"
#include "psx/types.h"

/* The first unit whose has_turn is set (the unit taking its turn), or 0. */
unit_stats_t* battle_unit_find_active_data_pointer(void) {
    s32 i = 0;
    unit_stats_t* unit = g_battle_unit_stats;
    for (; i < BATTLE_UNIT_SLOT_COUNT; i++, unit++) {
        if (unit->has_turn != 0) {
            return unit;
        }
    }
    return 0;
}
