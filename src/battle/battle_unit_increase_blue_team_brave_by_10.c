#include "fft/battle.h"
#include "fft/unit_slots.h"

void battle_unit_increase_blue_team_brave_by_10(void) {
    battle_stats_t* unit;
    s32 unit_id;

    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        unit = battle_unit_get_stats_from_battle_id(unit_id);
        if ((unit->team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE) {
            unit->brave += 10;
            if (unit->brave >= 100) {
                unit->brave = 100;
            }
        }
    }
}
