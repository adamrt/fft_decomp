#include "fft/effect.h"

/* Return the original component with the largest signed-16 magnitude.
 *
 * Strict comparisons make earlier arguments win ties. The zero-trip scope
 * retains the retail register allocation without emitting control flow. */
s16 battle_effect_select_largest_magnitude_s16(s16 first, s16 second, s16 third) {
    s16 first_magnitude;
    s16 second_magnitude;
    s16 third_magnitude;
    s16 result;
    s32 third_is_larger;

    first_magnitude = first < 0 ? -first : first;
    second_magnitude = second < 0 ? -second : second;
    third_magnitude = third < 0 ? -third : third;

    if (first_magnitude < second_magnitude) {
        result = second;
        third_is_larger = second_magnitude < third_magnitude;
    } else {
        result = first;
        third_is_larger = first_magnitude < third_magnitude;
    }
    if (third_is_larger) {
        do {
            result = third;
        } while (0);
    }
    return result;
}
