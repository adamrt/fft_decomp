#include "fft/suzuki.h"
#include "psx/types.h"

/* Modulator step: square wave between +amplitude and -amplitude, the sign
 * tracked in SUZUKI_MODULATOR_NEGATED. Returns the current value. */
s32 suzuki_smd_modulator_step_alternating(suzuki_modulator_t* modulator) {
    if (--modulator->timer == 0) {
        modulator->timer = modulator->timer_reset;
        modulator->value = (modulator->flags & SUZUKI_MODULATOR_NEGATED) ? -modulator->amplitude : modulator->amplitude;
        modulator->flags ^= SUZUKI_MODULATOR_NEGATED;
    }
    return modulator->value;
}
