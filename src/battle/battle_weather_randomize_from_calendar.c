#include "fft/battle.h"
#include "psx/libc.h"
#include "psx/types.h"

enum {
    BATTLE_WEATHER_CALENDAR_PERIOD_COUNT = 4,
    BATTLE_WEATHER_RANDOM_VARIANT_COUNT = 10,
};

/* Randomizes the battle's weather and time of day for the current date.
 *
 * The calendar cutoffs divide the year into four weather periods. Each row of
 * signed adjustments is indexed by a uniform `rand()` bucket and applied to
 * the current weather strength before it is clamped to 0..4.
 */
void battle_weather_randomize_from_calendar(void) {
    u8 period_end_month[BATTLE_WEATHER_CALENDAR_PERIOD_COUNT] = { 2, 5, 7, 11 };
    u8 period_end_day[BATTLE_WEATHER_CALENDAR_PERIOD_COUNT] = { 19, 21, 23, 23 };
    u8 weather_period[BATTLE_WEATHER_CALENDAR_PERIOD_COUNT + 1] = { 3, 0, 2, 1, 3 };
    s8 weather_adjustments[BATTLE_WEATHER_CALENDAR_PERIOD_COUNT][BATTLE_WEATHER_RANDOM_VARIANT_COUNT] = {
        { -1, -1, -1, -1, -1, -1, 0, 0, 0, 1 },
        { -1, -1, -1, -1, -1, -1, -1, -1, 0, 1 },
        { -1, 0, 0, 0, 1, 1, 1, 1, 1, 1 },
        { -1, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
    };
    s32 month;
    s32 day;
    s32 weather;
    s32 period;
    s32 random_bucket;
    u8 selected_weather_period;

    month = battle_script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    day = battle_script_get_variable(EVENT_SCRIPT_VAR_DAY);
    weather = battle_script_get_variable(EVENT_SCRIPT_VAR_WEATHER);

    period = 0;
    while (period < BATTLE_WEATHER_CALENDAR_PERIOD_COUNT) {
        if (month < period_end_month[period]) {
            break;
        }
        if (month == period_end_month[period] && day < period_end_day[period]) {
            break;
        }
        period++;
    }

    selected_weather_period = weather_period[period];
    random_bucket = (rand() * BATTLE_WEATHER_RANDOM_VARIANT_COUNT) / 0x8000;
    weather += weather_adjustments[selected_weather_period][random_bucket];
    if (weather < BATTLE_WEATHER_NONE) {
        weather = BATTLE_WEATHER_NONE;
    } else if (weather >= BATTLE_WEATHER_SNOW) {
        weather = BATTLE_WEATHER_STRONG_STORM;
    }
    battle_script_set_variable(EVENT_SCRIPT_VAR_WEATHER, weather);

    if ((rand() * 3) / 0x8000 != 0) {
        battle_script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, 0);
    } else {
        battle_script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, 1);
    }
}
