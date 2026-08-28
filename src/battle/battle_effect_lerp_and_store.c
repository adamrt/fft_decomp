#include "fft/battle_effect.h"

s32 battle_effect_lerp_and_store(s32 start, s32 end, s32 factor, s32* out) {
    s32 result;

    result = ((end - start) * factor >> 8) + start;
    *out = result;
    return result;
}
