#include "fft/battle.h"
#include "fft/script.h"

/* Linear interpolation from start to end at fraction / value, returned in 20.12
 * fixed point: start * ONE + (end - start) * fraction * ONE / value. */
s32 script_interpolate_range_fixed12(s32 start, s32 end, s32 fraction, s32 value) {
    return mul_div_s64(end - start, fraction * ONE, value) + (start * ONE);
}
