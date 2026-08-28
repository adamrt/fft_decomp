#include "fft/battle.h"
#include "psx/types.h"

s32 battle_script_interpolate_range_fixed12(s32 start, s32 end, s32 fraction, s32 value) {
    return battle_mul_div_s64(end - start, fraction << 12, value) + (start << 12);
}
