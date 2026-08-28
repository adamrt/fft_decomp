#include "fft/battle.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

battle_stats_t* battle_unit_find_active_data_pointer(void) {
    s32 i = 0;
    battle_stats_t* unit = g_battle_unit_stats;
    do {
        if (unit->has_turn != 0) {
            return unit;
        }
        i++;
        unit++;
    } while (i < BATTLE_UNIT_SLOT_COUNT);
    return (battle_stats_t*)0;
}
