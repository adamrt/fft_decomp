/* Advancing the calendar by days, with years, birthdays and ages, and proposition progress. */

#include "fft/script/script_variable.h"
#include "fft/world/brave_story.h"
#include "fft/world/calendar.h"
#include "fft/world/proposition.h"
#include "psx/types.h"

extern calendar_date_t g_brave_story_birthday_table[64];

calendar_date_t* g_brave_story_birthdays = g_brave_story_birthday_table;

/*
 * Advance the calendar by a script-requested number of days.
 *
 * The only caller passes EVENT_SCRIPT_VAR_DATE_ADVANCE. Whole years are added
 * to EVENT_SCRIPT_VAR_UNKNOWN_67 and to every Brave Story age;
 * the remaining days move the month/day variables forward, adding one more
 * year when the day of the year passes 365. A character whose birthday falls
 * within the remaining days gains a year, and ages are capped at 99. Entry 0
 * is the player character, whose birthday lives in the
 * EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH and _DAY variables, as in
 * advance_birthdays.
 *
 * Every active proposition not yet returned returns when its elapsed days plus
 * the advanced days reach its assigned days (elapsed becomes the assigned
 * count); a recalled one returns with one more elapsed day. The unfinished
 * branch leaves elapsed days unchanged. Offer countdowns drop by the remaining
 * days modulo 365, floored at 0, keeping their failure bits.
 *
 * The birthday table's element type must not be const: a const element makes
 * the reads unchanging, and the scheduler then hoists the second table read
 * above the store of the first. The loop's `i = 0` must sit at the loop, not
 * at function entry, for the first call's `li a0,0x67` to precede the
 * reciprocal-division `mult`.
 */
void calendar_advance_days(s32 days) {
    s32 i;
    s32 years;
    calendar_date_t* birthdays;
    s32 today;
    s32 month;
    s32 day;
    s32 until_birthday;

    birthdays = g_brave_story_birthdays;
    years = days / 365;
    script_variable_set(EVENT_SCRIPT_VAR_UNKNOWN_67, script_variable_get(EVENT_SCRIPT_VAR_UNKNOWN_67) + years);
    days %= 365;
    month = script_variable_get(EVENT_SCRIPT_VAR_MONTH);
    day = script_variable_get(EVENT_SCRIPT_VAR_DAY);
    today = calendar_date_to_day_index(month, day);
    for (i = 0; i < 64; i++) {
        g_brave_story_character_ages[i] += years;
        if (i == 0) {
            month = script_variable_get(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH);
            day = script_variable_get(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY);
        } else {
            month = birthdays[i].month;
            day = birthdays[i].day;
        }
        /* Only birthdays later in this year count: an advance that runs past
         * 31 December misses those early in the new year (retail quirk). */
        until_birthday = calendar_date_to_day_index(month, day) - today;
        if (until_birthday > 0 && days >= until_birthday) {
            g_brave_story_character_ages[i]++;
        }
        if (g_brave_story_character_ages[i] >= 100) {
            g_brave_story_character_ages[i] = 99;
        }
    }

    today += days;
    if (today >= 365) {
        script_variable_set(EVENT_SCRIPT_VAR_UNKNOWN_67, script_variable_get(EVENT_SCRIPT_VAR_UNKNOWN_67) + 1);
        today -= 365;
    }
    calendar_day_index_to_date(today, &month, &day);
    script_variable_set(EVENT_SCRIPT_VAR_MONTH, month);
    script_variable_set(EVENT_SCRIPT_VAR_DAY, day);

    for (i = 0; i < g_proposition_count; i++) {
        if (!(g_proposition_active[i].flags & PROPOSITION_PROGRESS_RETURNED)) {
            if (g_proposition_active[i].elapsed_days + days >= g_proposition_active[i].assigned_days) {
                g_proposition_active[i].elapsed_days = g_proposition_active[i].assigned_days;
                g_proposition_active[i].flags |= PROPOSITION_PROGRESS_RETURNED;
            } else if (g_proposition_active[i].flags & PROPOSITION_PROGRESS_RECALLED) {
                g_proposition_active[i].elapsed_days++;
                g_proposition_active[i].flags |= PROPOSITION_PROGRESS_RETURNED;
            }
        }
    }

    for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (g_proposition_states[i] & PROPOSITION_STATE_DAYS_MASK) {
            day = (g_proposition_states[i] & PROPOSITION_STATE_DAYS_MASK) - days;
            if (day < 0) {
                day = 0;
            }
            g_proposition_states[i]
                = (g_proposition_states[i] & (PROPOSITION_STATE_FAILURE | PROPOSITION_STATE_BAD_FAILURE))
                | (day & PROPOSITION_STATE_DAYS_MASK);
        }
    }
}
