/* Proposition rows, availability and the daily calendar advance. */

#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Resolve record `record` of the proposition data file: a table of offsets
 * relative to the file's start. */
void* wldcore_proposition_get_data_pointer(wldcore_proposition_data_record_e record) {
    return (u8*)g_wldcore_proposition_data_base + g_wldcore_proposition_data_base[record];
}

/* Expand one 23-byte proposition row into 16-bit fields, then remap the
 * two fee fields through the 16-bit table of record 1. */
void wldcore_unpack_proposition_row(wldcore_proposition_fields_t* out, s32 index) {
    u8* rows;
    u16* field_lookup;
    s32 i;
    u16* fields;

    rows = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_ROWS);
    fields = &out->category_index;
    for (i = 0; i < 23; i++) {
        fields[i] = rows[index * 23 + i];
    }
    field_lookup = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_FEES);
    out->fee_a = field_lookup[out->fee_a];
    out->fee_b = field_lookup[out->fee_b];
}

/* Whether proposition `index` is offered at `location`: the row's location
 * (0 for any), then each condition its flags select. The status condition
 * tests bit 2 of script variable 0x360 + condition_value. */
s32 wldcore_proposition_is_available_at_location(s32 index, s32 location) {
    wldcore_proposition_fields_t entry;
    s32 month;
    s32 day;

    wldcore_unpack_proposition_row(&entry, index);
    if (entry.location != 0) {
        if (location != entry.location) {
            return 0;
        }
    }
    if (entry.condition_flags & WLDCORE_PROPOSITION_CONDITION_SCRIPT_VAR_6F) {
        if (script_get_variable(EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY) < entry.required_script_var_6f) {
            return 0;
        }
    }
    if (entry.condition_flags & WLDCORE_PROPOSITION_CONDITION_MONTH) {
        month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
        day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
        wldcore_calendar_convert_date_to_zodiac(&month, &day);
        if (month != entry.condition_value) {
            return 0;
        }
    }
    if (entry.condition_flags & WLDCORE_PROPOSITION_CONDITION_STATUS) {
        if ((script_get_variable(entry.condition_value + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
                & WLDCORE_PROPOSITION_VARIABLE_COMPLETED)
            == 0) {
            return 0;
        }
    }
    return 1;
}

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

/* Converts a calendar date in place into a zodiac sign number (1 = Aries) and
 * the day within that sign. Invalid dates play sound effect 5 and reset to 1. */
void wldcore_calendar_convert_date_to_zodiac(s32* month_ptr, s32* day_ptr) {
    s32 index;
    s32 delta;
    s32 previous_month;
    calendar_date_t* start_dates;
    u8* start_days;
    calendar_date_t* previous_dates;

    index = 0;
    if (*month_ptr < 1 || *month_ptr > 12 || *day_ptr < 1 || *day_ptr > 31) {
        wldcore_sound_play_effect(MAIN_SFX_INVALID);
        *day_ptr = 1;
        *month_ptr = 1;
        return;
    }
    start_dates = g_wldcore_calendar_zodiac_start_dates;
    while (start_dates[index].month != *month_ptr) {
        index = (index + 1) % 12;
    }
    /* Direct .day indexing folds the two original indexed addresses into one. */
    start_days = &g_wldcore_calendar_zodiac_start_dates[0].day;
    if (*day_ptr < start_days[index * sizeof(calendar_date_t)]) {
        index = (index + 11) % 12;
        *month_ptr = index + 1;
        /* Separate month-table local preserves the original register allocation. */
        previous_dates = g_wldcore_calendar_zodiac_start_dates;
        previous_month = previous_dates[index].month;
        delta = start_days[index * sizeof(calendar_date_t)];
        delta = g_unit_birthday_month_lengths[previous_month - 1] - delta + 1;
        *day_ptr += delta;
    } else {
        *month_ptr = index + 1;
        *day_ptr = *day_ptr - start_days[index * sizeof(calendar_date_t)] + 1;
    }
}

/*
 * The daily birthday sweep, run by wldcore_calendar_advance_day after the
 * calendar rolls over: every brave-story character whose stored birthday is
 * today gets a year older, capped at 99.
 *
 * Entry 0 is the player character. Its row in the birthday table is a
 * placeholder (1/1); the real birthday lives in script variables 0x5f (month)
 * and 0x60 (day), written when the player picks one at game start, so entry 0
 * is compared against those two variables and the remaining 63 entries
 * against the table. The table is a 64 x 2 array of month/day byte pairs
 * reached through the pointer cell at 0x80096a48 (it points at 0x80097028,
 * whose 128 bytes read as plausible month/day pairs throughout).
 *
 * The age column is walked one byte at a time from index 1, which is why the
 * loop's base materialises as the biased constant 0x80057f35; entry 0 is the
 * only absolute access to 0x80057f34.
 */
void wldcore_advance_brave_story_birthdays(void) {
    const u8(*birthdays)[2] = g_wldcore_brave_story_birthdays;
    s32 day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
    s32 month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    s32 birth_month = script_get_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH);
    s32 birth_day = script_get_variable(EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY);
    u8 age;
    s32 i;

    if (month == birth_month && day == birth_day) {
        age = g_main_brave_story_character_ages[0];
        if (age < 99) {
            g_main_brave_story_character_ages[0] = age + 1;
        }
    }

    for (i = 1; i < 64; i++) {
        if (month == birthdays[i][0] && day == birthdays[i][1]) {
            if (g_main_brave_story_character_ages[i] < 99) {
                g_main_brave_story_character_ages[i]++;
            }
        }
    }
}
