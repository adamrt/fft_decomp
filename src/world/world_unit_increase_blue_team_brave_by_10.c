#include "fft/battle.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

/* Grants +10 Brave, capped at 100, to every blue-team battle unit. */
void world_unit_increase_blue_team_brave_by_10(void) {
    s32 i;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        battle_stats_t* unit = battle_unit_get_stats_from_battle_id(i);
        if ((unit->team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE) {
            unit->brave += 10;
            if (unit->brave >= 100) {
                unit->brave = 100;
            }
        }
    }
}
