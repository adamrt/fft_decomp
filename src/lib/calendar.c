/* Convert a month and day to a zero-based day-of-year index and back. WLDCORE's calendar and saved
 * birthdays (party_unit_t.birthday_day) use these indexes; OPEN only converts birthdays. */

#include "fft/world/calendar.h"
#include "psx/types.h"

/* Zero-based day of the year of a 1-based month and day (1 January is 0), with
 * February at 28 days. */
s32 calendar_date_to_day_index(s32 month, s32 day) {
    s32 index;
    s32 elapsed_days;
    s32 result;

    month--;
    elapsed_days = 0;
    for (index = 0; index < month; index++) {
        elapsed_days += g_calendar_month_lengths[index];
    }

    /* The bytes need this temporary; `elapsed_days - 1 + day` reorders the adds. */
    result = elapsed_days - 1;
    return result + day;
}

/* OPEN only needs the birthday-to-day-index conversion; WLDCORE also reads saved day indexes. */
#ifdef FFT_WLDCORE
/* 0-based day-of-year index (party_unit_t.birthday_day) to 1-based month
 * and day. */
void calendar_day_index_to_date(s32 day_of_year, s32* output_month, s32* output_day) {
    s32 month;
    s32 days_in_month;

    for (month = 0; month < 12; month++) {
        days_in_month = g_calendar_month_lengths[month];
        if (day_of_year < days_in_month) {
            break;
        }
        day_of_year -= days_in_month;
    }
    *output_month = month + 1;
    *output_day = day_of_year + 1;
}

#endif
