#include "fft/suzuki.h"
#include "psx/types.h"

/* Modulator step: sawtooth; value ramps by amplitude every step and resets
 * to zero when the timer expires. */
s32 suzuki_smd_modulator_step_sawtooth(suzuki_modulator_t* modulator) {
    if (--modulator->timer == 0) {
        modulator->value = 0;
        modulator->timer = modulator->timer_reset;
    } else {
        modulator->value += modulator->amplitude;
    }
    return modulator->value;
}
