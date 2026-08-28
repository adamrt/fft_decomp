#include "fft/wldcore.h"
#include "psx/types.h"

/* Inverse of wldcore_day_of_year_to_month_day: 1-based month and day to a
 * 0-based day-of-year index (party_data_t.birthday_day). */
s32 wldcore_month_day_to_day_of_year(s32 month, s32 day) {
    s32 index;
    s32 days_before_month;
    s32 day_of_year;

    month -= 1;
    days_before_month = 0;
    for (index = 0; index < month; index++)
        days_before_month += g_wldcore_days_per_month[index];
    day_of_year = days_before_month - 1;
    return day_of_year + day;
}
