#include "psx/types.h"

/* Counts magnitude digits (at least one), not a minus sign. */
s32 world_text_count_decimal_digits(s32 value) {
    s32 digits;

    value = value / 10;
    digits = 1;
    while (value != 0) {
        value = value / 10;
        digits += 1;
    }
    return digits;
}
