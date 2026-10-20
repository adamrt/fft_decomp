/* The world-map calendar converts month/day dates to zero-based year offsets and back. */

#include "fft/calendar.h"
#include "psx/types.h"

/* Zero-based day of the year of a 1-based month and day (1 January is 0), with
 * February at 28 days. */
s32 calendar_day_of_year_calculate(s32 month, s32 day) {
    s32 index;
    s32 elapsed_days;
    s32 result;

    month--;
    elapsed_days = 0;
    index = 0;
    while (index < month) {
        elapsed_days += g_calendar_month_lengths[index];
        index++;
    }

    result = elapsed_days - 1;
    return result + day;
}

/* OPEN only needs the birthday-to-day-index conversion; WLDCORE also reads saved day indexes. */
#ifdef FFT_WLDCORE
/* 0-based day-of-year index (party_unit_t.birthday_day) to 1-based month
 * and day. */
void calendar_month_and_day_calculate(s32 day_of_year, s32* output_month, s32* output_day) {
    s32 month = 0;
    s32 days_in_month;

    while (1) {
        days_in_month = g_calendar_month_lengths[month];
        if (day_of_year < days_in_month) {
            break;
        }
        month += 1;
        day_of_year -= days_in_month;
        if (month >= 12) {
            break;
        }
    }
    *output_month = month + 1;
    *output_day = day_of_year + 1;
}

#endif
