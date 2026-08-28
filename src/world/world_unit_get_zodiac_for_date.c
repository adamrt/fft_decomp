#include "fft/world.h"

/*
 * Look up a zodiac value from month/day upper limits.
 *
 * The 13 ordered limits include both Capricorn portions of the year.
 * No date validation is performed; exhausting the 13 limits reads 31 from
 * the following month-length data, not a valid extra zodiac entry.
 */
s32 world_unit_get_zodiac_for_date(s32 month, s32 day) {
    s32 i;

    for (i = 0; i < WORLD_ZODIAC_DATE_LIMIT_COUNT; i++) {
        if (month < g_world_zodiac_date_limits[i].month) {
            break;
        }
        if (month == g_world_zodiac_date_limits[i].month) {
            if (g_world_zodiac_date_limits[i].day >= day) {
                break;
            }
        }
    }
    return g_world_zodiac_date_limits[i].zodiac;
}
