#include "fft/suzuki.h"

/* Linear ramp: value slides by step until count expires, then snaps to
 * target. */
void suzuki_smd_step_ramp(suzuki_ramp_t* ramp) {
    if (--ramp->count != 0) {
        ramp->value += ramp->step;
    } else {
        ramp->value = ramp->target << 16;
    }
}
