#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

/* Increment the battle casualty or injured counter for a unit, capped at 9999. */
void main_unit_increase_casualty_counters(unit_stats_t* unit) {
    s32 id;
    s32 count;

    id = (unit->initial_team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE ? EVENT_SCRIPT_VAR_INJURED
                                                                           : EVENT_SCRIPT_VAR_CASUALTIES;
    count = script_get_variable(id);
    if (count < 0x270F) {
        count++;
    }
    script_set_variable(id, count);
}
