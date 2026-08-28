#include "fft/open.h"
#include "psx/types.h"

s32 open_birthday_calculate_day_index(s32 month, s32 day) {
    s32 index;
    s32 elapsed_days;
    s32 result;

    month--;
    elapsed_days = 0;
    index = 0;
    while (index < month) {
        elapsed_days += g_open_birthday_month_lengths[index];
        index++;
    }

    result = elapsed_days - 1;
    return result + day;
}
