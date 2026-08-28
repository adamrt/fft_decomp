#include "fft/world.h"
#include "psx/types.h"

s32 world_script_interpolate_range_fixed12(s32 start, s32 end, s32 fraction, s32 value) {
    return world_mul_div_64(end - start, fraction << 12, value) + (start << 12);
}
