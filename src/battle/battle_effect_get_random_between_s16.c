#include "psx/types.h"

/* Return a signed 16-bit value in the half-open interval between two bounds. */
s16 battle_effect_get_random_between_s16(s16 minimum, s16 maximum) {
    s16 result;

    if (minimum == maximum) {
        result = minimum;
    } else if (maximum < minimum) {
        result = maximum + rand() % (minimum - maximum);
    } else {
        result = minimum + rand() % (maximum - minimum);
    }

    return result;
}
