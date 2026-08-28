#include "psx/gte.h"
#include "psx/types.h"

/* Cosine-eased interpolation from start to end at current/duration; the same
 * curve as battle_effect_interpolate_svector_cos, on a single s32. */
s32 battle_effect_interpolate_s32_cos(s32 start, s32 end, s32 duration, s32 current) {
    s32 difference;
    s32 scaled_difference;

    difference = end - start;
    scaled_difference = difference * (ONE - rcos((current << 11) / duration));
    return start + scaled_difference / 0x2000;
}
