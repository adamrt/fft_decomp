#include "fft/battle.h"
#include "fft/main.h"
#include "psx/types.h"

/* Increment the battle casualty or injured counter for a unit, capped at 9999. */
void main_unit_increase_casualty_counters(battle_stats_t* unit) {
    s32 id;
    s32 count;

    id = (unit->initial_team_flags & BATTLE_TEAM_MASK) != 0 ? 0x61 : 0x62;
    count = battle_script_get_variable(id);
    if (count < 0x270F) {
        count++;
    }
    battle_script_set_variable(id, count);
}
