#include "fft/unit.h"
#include "psx/types.h"

/* Zero-based day of the year of a 1-based month and day (1 January is 0), with
 * February at 28 days. */
s32 unit_birthday_calculate_day_of_year(s32 month, s32 day) {
    s32 index;
    s32 elapsed_days;
    s32 result;

    month--;
    elapsed_days = 0;
    index = 0;
    while (index < month) {
        elapsed_days += g_unit_birthday_month_lengths[index];
        index++;
    }

    result = elapsed_days - 1;
    return result + day;
}
