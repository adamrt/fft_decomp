#include "fft/open.h"

/* Convert a calendar month/day into a zodiac sign number (1 = Aries) and the
 * 1-based day within that sign.
 *
 * `span` first holds the previous sign's start day and then the number of
 * days from it to the end of its month; computing it in two steps keeps GCC
 * from folding the `+ 1` into the sum with `*day`. */
void open_birthday_convert_to_zodiac_position(s32* month, s32* day) {
    s32 index;
    s32 length;
    s32 span;

    index = 0;
    while (g_open_birthday_zodiac_months[index * 2] != *month) {
        index = (index + 1) % ZODIAC_SIGN_ORDINARY_COUNT;
    }
    if (*day < g_open_birthday_zodiac_months[index * 2 + 1]) {
        index = (index + ZODIAC_SIGN_ORDINARY_COUNT - 1) % ZODIAC_SIGN_ORDINARY_COUNT;
        *month = index + 1;
        length = (g_open_birthday_month_lengths - 1)[g_open_birthday_zodiac_months[index * 2]];
        span = g_open_birthday_zodiac_months[index * 2 + 1];
        span = length - span + 1;
        *day += span;
        return;
    }
    *month = index + 1;
    *day = *day - g_open_birthday_zodiac_months[index * 2 + 1] + 1;
}
