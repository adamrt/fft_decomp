#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"

s32 wldcore_month_day_to_day_of_year(s32 month, s32 day);
void wldcore_day_of_year_to_month_day(s32 day_of_year, s32* output_month, s32* output_day);

/*
 * Advance the calendar by a script-requested number of days.
 *
 * The only caller passes script variable 0x54 (DATE_ADVANCE). Whole years
 * are added to script variable 0x67 and to every brave-story age; the
 * remaining days move the month/day variables forward, adding one more to
 * 0x67 when the day-of-year index passes 365. A character whose birthday
 * falls within the remaining days gains a year, and ages are capped at 99.
 * Entry 0 is the player character, whose birthday lives in script variables
 * 0x5f (month) and 0x60 (day), as in wldcore_advance_brave_story_birthdays.
 *
 * Open propositions complete when elapsed plus the advanced days reaches the
 * assigned duration (elapsed is set to the duration); otherwise flag 0x02
 * completes them with elapsed incremented once, and the rest keep their
 * elapsed count unchanged. Nonzero six-bit proposition countdowns drop by the
 * advanced days, floored at 0, keeping their high 2 bits.
 *
 * The birthday table's element type must not be const: a const element makes
 * the reads unchanging, and the scheduler then hoists the second table read
 * above the store of the first. The loop's `i = 0` must sit at the loop, not
 * at function entry, for the first call's `li a0,0x67` to precede the
 * reciprocal-division `mult`.
 */
void wldcore_advance_calendar_days(s32 days) {
    s32 i;
    s32 years;
    u8(*birthdays)[2];
    s32 today;
    s32 month;
    s32 day;
    s32 until_birthday;
    u8* counter;

    birthdays = g_wldcore_brave_story_birthdays;
    years = days / 365;
    world_script_set_variable(
        EVENT_SCRIPT_VAR_UNKNOWN_67, world_script_get_variable(EVENT_SCRIPT_VAR_UNKNOWN_67) + years);
    days %= 365;
    month = world_script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    day = world_script_get_variable(EVENT_SCRIPT_VAR_DAY);
    today = wldcore_month_day_to_day_of_year(month, day);
    for (i = 0; i < 64; i++) {
        g_main_brave_story_character_ages[i] += years;
        if (i == 0) {
            month = world_script_get_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH);
            day = world_script_get_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY);
        } else {
            month = birthdays[i][0];
            day = birthdays[i][1];
        }
        until_birthday = wldcore_month_day_to_day_of_year(month, day) - today;
        if (until_birthday > 0 && days >= until_birthday) {
            g_main_brave_story_character_ages[i] += 1;
        }
        if (g_main_brave_story_character_ages[i] >= 100) {
            g_main_brave_story_character_ages[i] = 99;
        }
    }
    today += days;
    if (today >= 365) {
        world_script_set_variable(
            EVENT_SCRIPT_VAR_UNKNOWN_67, world_script_get_variable(EVENT_SCRIPT_VAR_UNKNOWN_67) + 1);
        today -= 365;
    }
    wldcore_day_of_year_to_month_day(today, &month, &day);
    world_script_set_variable(EVENT_SCRIPT_VAR_MONTH, month);
    world_script_set_variable(EVENT_SCRIPT_VAR_DAY, day);
    for (i = 0; i < g_main_save_proposition_count; i++) {
        if (!(g_main_active_propositions[i].flags & 4)) {
            if (g_main_active_propositions[i].elapsed_days + days >= g_main_active_propositions[i].assigned_days) {
                g_main_active_propositions[i].elapsed_days = g_main_active_propositions[i].assigned_days;
                g_main_active_propositions[i].flags |= 4;
            } else if (g_main_active_propositions[i].flags & 2) {
                g_main_active_propositions[i].elapsed_days++;
                g_main_active_propositions[i].flags |= 4;
            }
        }
    }
    counter = g_main_proposition_states;
    do {
        if (*counter & 0x3f) {
            day = (*counter & 0x3f) - days;
            if (day < 0) {
                day = 0;
            }
            *counter = (*counter & 0xc0) | (day & 0x3f);
        }
        counter++;
    } while ((s32)counter < (s32)(g_main_proposition_states + 96)); /* Target uses signed SLT. */
}
