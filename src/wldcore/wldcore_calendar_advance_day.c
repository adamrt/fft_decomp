#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/*
 * Advance the calendar by 1 day and run the daily updates.
 *
 * Script variable 0x67 increases on March 21 and is capped at 99.
 * The target does not validate the incoming month or day.
 */
void wldcore_calendar_advance_day(void) {
    s32 month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    s32 day = script_get_variable(EVENT_SCRIPT_VAR_DAY) + 1;

    if (g_unit_birthday_month_lengths[month - 1] < day) {
        month++;
        if (month == 13) {
            month = 1;
        }
        day = 1;
    }
    script_set_variable(EVENT_SCRIPT_VAR_MONTH, month);
    script_set_variable(EVENT_SCRIPT_VAR_DAY, day);
    if (month == 3 && day == 21) {
        s32 value = script_get_variable(EVENT_SCRIPT_VAR_UNKNOWN_67) + 1;
        if (value >= 100) {
            value = 99;
        }
        script_set_variable(EVENT_SCRIPT_VAR_UNKNOWN_67, value);
    }
    wldcore_proposition_advance_daily_counters();
    wldcore_advance_brave_story_birthdays();
    world_unit_update_monster_breeding(1);
}
