#include "fft/suzuki.h"
#include "psx/types.h"

/* Modulator step: square wave between 0 and amplitude, toggling each time
 * the timer expires. Returns the current value. */
s32 suzuki_smd_modulator_step_square(suzuki_modulator_t* modulator) {
    if (--modulator->timer == 0) {
        modulator->timer = modulator->timer_reset;
        modulator->value = modulator->value == 0 ? modulator->amplitude : 0;
    }
    return modulator->value;
}
