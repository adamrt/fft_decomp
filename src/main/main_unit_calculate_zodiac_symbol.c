#include "fft/main.h"
#include "psx/types.h"

/* Return the zodiac sign for a day-of-year birthday.
 *
 * Counts the sign boundaries at or before the day, then rotates the count
 * into zodiac order with (count + 9) % 12. */
s32 main_unit_calculate_zodiac_symbol(u32 birthday) {
    s32 zodiac;
    s32 index;
    s32 sign;
    s32 shifted;
    const u16* day_limit;
    u16 day;

    zodiac = 0;
    index = 0;
    birthday &= 0xffff;
    day_limit = g_zodiac_day_limits;

    /* A goto loop: as any while/for/do loop, GCC's loop pass reverses the unused
     * counter into a count-down. The explicit division below reproduces the
     * target's temp allocation. */
next:
    day = *day_limit++;
    index++;
    zodiac += birthday >= day;
    if (index < ZODIAC_SIGN_ORDINARY_COUNT)
        goto next;
    shifted = zodiac + 9;
    zodiac = shifted / ZODIAC_SIGN_ORDINARY_COUNT;
    sign = shifted >> 31;
    /* The helper's ABI returns an int, normalized to the target's unsigned
     * 16-bit result before leaving the function. */
    return (u16)(shifted - zodiac * ZODIAC_SIGN_ORDINARY_COUNT);
}
