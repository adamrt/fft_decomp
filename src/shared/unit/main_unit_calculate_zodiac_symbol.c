#include "fft/main.h"
#include "psx/types.h"

/* Return the zodiac sign for a day-of-year birthday.
 *
 * Counts the sign boundaries at or before the day, then rotates the count
 * into zodiac order with (count + 9) % 12. */
s32 main_unit_calculate_zodiac_symbol(u16 birthday) {
    s32 zodiac;
    s32 i;

    zodiac = 0;
    for (i = 0; i < ZODIAC_SIGN_ORDINARY_COUNT; i++) {
        zodiac += birthday >= g_main_zodiac_day_limits[i];
    }
    zodiac = (zodiac + 9) % ZODIAC_SIGN_ORDINARY_COUNT;
    return (u16)zodiac;
}
