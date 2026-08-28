#include "psx/types.h"

s32 battle_text_count_decimal_digits(s32 value) {
    s32 digits;

    value /= 10;
    digits = 1;
    if (value != 0) {
        do {
            value /= 10;
            digits++;
        } while (value != 0);
    }
    return digits;
}
