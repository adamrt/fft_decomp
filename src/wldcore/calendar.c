/* World-map calendar changes advance birthdays and pack dates into saved proposition records. */

#include "fft/calendar.h"
#include "fft/bits.h"
#include "fft/brave_story.h"
#include "fft/formation.h"
#include "fft/party.h"
#include "fft/proposition.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/unit_party.h"
#include "psx/types.h"

/* Twelve {month, day} pairs, Aries first. */
extern calendar_date_t g_calendar_zodiac_start_dates[ZODIAC_SIGN_ORDINARY_COUNT];

static void birthdays_advance(void);

/*
 * Advance the calendar by 1 day and run the daily updates.
 *
 * EVENT_SCRIPT_VAR_UNKNOWN_67 increases on 21 March (the start of Aries) and
 * is capped at 99; calendar_days_advance raises it at the year's end
 * instead, without the cap. The target does not validate the incoming month
 * or day.
 */
void calendar_day_advance(void) {
    s32 month = script_variable_get(EVENT_SCRIPT_VAR_MONTH);
    s32 day = script_variable_get(EVENT_SCRIPT_VAR_DAY) + 1;

    if (g_calendar_month_lengths[month - 1] < day) {
        month++;
        if (month == 13) {
            month = 1;
        }
        day = 1;
    }
    script_variable_set(EVENT_SCRIPT_VAR_MONTH, month);
    script_variable_set(EVENT_SCRIPT_VAR_DAY, day);
    if (month == 3 && day == 21) {
        s32 value = script_variable_get(EVENT_SCRIPT_VAR_UNKNOWN_67) + 1;
        if (value >= 100) {
            value = 99;
        }
        script_variable_set(EVENT_SCRIPT_VAR_UNKNOWN_67, value);
    }
    proposition_day_advance();
    birthdays_advance();
    breeding_update(1);
}

/* Converts a calendar date in place into a zodiac sign number (1 = Aries) and
 * the day within that sign. An invalid date plays SOUND_SFX_INVALID and
 * becomes 1/1. */
void calendar_date_to_zodiac_convert(s32* month_ptr, s32* day_ptr) {
    s32 index = 0;
    s32 delta;
    s32 previous_month;
    calendar_date_t* start_dates;
    u8* start_days;
    calendar_date_t* previous_dates;

    if (*month_ptr < 1 || *month_ptr > 12 || *day_ptr < 1 || *day_ptr > 31) {
        sound_play_effect(SOUND_SFX_INVALID);
        *day_ptr = 1;
        *month_ptr = 1;
        return;
    }
    start_dates = g_calendar_zodiac_start_dates;
    while (start_dates[index].month != *month_ptr) {
        index = (index + 1) % 12;
    }
    /* Direct .day indexing folds the two original indexed addresses into one. */
    start_days = &g_calendar_zodiac_start_dates[0].day;
    if (*day_ptr < start_days[index * sizeof(calendar_date_t)]) {
        index = (index + 11) % 12;
        *month_ptr = index + 1;
        /* Separate month-table local preserves the original register allocation. */
        previous_dates = g_calendar_zodiac_start_dates;
        previous_month = previous_dates[index].month;
        delta = start_days[index * sizeof(calendar_date_t)];
        delta = g_calendar_month_lengths[previous_month - 1] - delta + 1;
        *day_ptr += delta;
    } else {
        *month_ptr = index + 1;
        *day_ptr = *day_ptr - start_days[index * sizeof(calendar_date_t)] + 1;
    }
}

/*
 * The daily birthday sweep, run by calendar_day_advance after the
 * calendar rolls over: every Brave-story character whose stored birthday is
 * today gets a year older, capped at 99.
 *
 * Entry 0 is the player character. Its row in the birthday table is a
 * placeholder (1/1); the real birthday lives in
 * EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH and _DAY, written when the player picks
 * one at game start, so entry 0 is compared against those two variables and
 * the remaining 63 entries against the table. The table contains 64 calendar dates
 * (g_brave_story_birthday_table), reached through
 * the pointer g_brave_story_birthdays.
 *
 * The age column is walked one byte at a time from index 1, which is why the
 * loop's base materializes as g_brave_story_character_ages + 1; entry 0
 * is the only absolute access to the array.
 */
static void birthdays_advance(void) {
    const calendar_date_t* birthdays = g_brave_story_birthdays;
    s32 day = script_variable_get(EVENT_SCRIPT_VAR_DAY);
    s32 month = script_variable_get(EVENT_SCRIPT_VAR_MONTH);
    s32 birth_month = script_variable_get(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH);
    s32 birth_day = script_variable_get(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY);
    u8 age;
    s32 i;

    if (month == birth_month && day == birth_day) {
        age = g_brave_story_character_ages[0];
        if (age < 99) {
            g_brave_story_character_ages[0] = age + 1;
        }
    }

    for (i = 1; i < 64; i++) {
        if (month == birthdays[i].month && day == birthdays[i].day) {
            if (g_brave_story_character_ages[i] < 99) {
                g_brave_story_character_ages[i]++;
            }
        }
    }
}

/* WLDCORE's wrapper for party_unit_get: the roster record of slot `party_index`. */
party_unit_t* party_roster_unit_get(s32 party_index) {
    return party_unit_get(party_index);
}

/* Writes one nine-bit record: bits 0-4 from source->day, bits 5-8 from
 * source->month. Records are packed nine bits apart in the bit array. */
void calendar_date_write(u32* bits, s32 record, calendar_date_t* source) {
    s32 base = record * 9;
    s32 mask;
    s32 i;

    mask = 1;
    for (i = 0; i < 5; i++) {
        bits_value_set(bits, base + i, source->day & mask);
        mask <<= 1;
    }
    mask = 1;
    for (i = 0; i < 4; i++) {
        /* The separate temporary keeps the target's add order: folding
         * base + (i + 5) in one expression hoists base + 5 out of the loop. */
        s32 index = i + 5;

        bits_value_set(bits, base + index, source->month & mask);
        mask <<= 1;
    }
}

/* Reads one nine-bit record: bits 0-4 into dest->day, bits 5-8 into dest->month.
 * Records are packed nine bits apart in the bit array, matching
 * calendar_date_write. A field that decodes to zero or beyond its
 * range falls back to 1. */
void calendar_date_read(u32* bits, s32 record, calendar_date_t* dest) {
    s32 base = record * 9;
    s32 mask;
    s32 i;

    mask = 1;
    dest->day = 0;
    for (i = 0; i < 5; i++) {
        if (bits_test(bits, base + i)) {
            dest->day |= mask;
        }
        mask <<= 1;
    }
    mask = 1;
    dest->month = 0;
    for (i = 0; i < 4; i++) {
        s32 index = i + 5;

        if (bits_test(bits, base + index)) {
            dest->month |= mask;
        }
        mask <<= 1;
    }
    if (dest->day == 0 || dest->day >= 32) {
        dest->day = 1;
    }
    if (dest->month == 0 || dest->month >= 13) {
        dest->month = 1;
    }
}
