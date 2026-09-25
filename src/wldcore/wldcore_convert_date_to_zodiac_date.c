#include "fft/wldcore.h"
#include "psx/types.h"

/* Converts a calendar date in place into a zodiac sign number (1 = Aries) and
 * the day within that sign.
 *
 * An out-of-range month or day plays sound effect 5 and resets both to 1.
 * The second table pointer for the previous sign's start month keeps the loop
 * base pointer's reference count low enough for global allocation to give the
 * loop-hoisted month copy the lower register, as the target does. */
void wldcore_convert_date_to_zodiac_date(s32* month_ptr, s32* day_ptr) {
    s32 index;
    s32 delta;
    s32 day;
    u8(*months)[2];
    u8(*days)[2];
    u8(*m2)[2];

    index = 0;
    if ((u32)(*month_ptr - 1) >= 12 || (u32)(*day_ptr - 1) >= 31) {
        wldcore_sound_play_effect(MAIN_SFX_INVALID);
        *day_ptr = 1;
        *month_ptr = 1;
        return;
    }
    months = g_wldcore_zodiac_start_dates;
    while (months[index][0] != *month_ptr) {
        index = (index + 1) % 12;
    }
    days = (u8(*)[2]) & g_wldcore_zodiac_start_dates[0][1];
    if (*day_ptr < days[index][0]) {
        index = (index + 11) % 12;
        *month_ptr = index + 1;
        m2 = g_wldcore_zodiac_start_dates;
        day = m2[index][0];
        delta = days[index][0];
        delta = g_wldcore_days_per_month[day - 1] - delta + 1;
        *day_ptr += delta;
    } else {
        *month_ptr = index + 1;
        *day_ptr = *day_ptr - days[index][0] + 1;
    }
}
