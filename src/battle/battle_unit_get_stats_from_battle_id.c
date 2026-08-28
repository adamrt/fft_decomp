#include "fft/battle.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

battle_stats_t* battle_unit_get_stats_from_battle_id(u32 id) {
    if (id >= BATTLE_UNIT_SLOT_COUNT) {
        return (battle_stats_t*)0;
    }
    return &g_battle_unit_stats[id];
}
