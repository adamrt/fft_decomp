#include "psx/types.h"

/*
 * Return the original component with the larger signed-16 magnitude.
 *
 * The strict comparison makes the first argument win ties. Negating -32768
 * wraps back to -32768 when stored as s16, matching the target's edge case.
 */
s16 battle_effect_select_larger_magnitude_s16(s16 first, s16 second) {
    s16 first_magnitude;
    s16 second_magnitude;
    s16 result;

    first_magnitude = first < 0 ? -first : first;
    second_magnitude = second < 0 ? -second : second;

    if (first_magnitude < second_magnitude) {
        result = second;
    } else {
        result = first;
    }
    return result;
}
