#include "fft/suzuki.h"
#include "psx/types.h"

/* Modulator step: triangle wave; the increment flips sign
 * (SUZUKI_MODULATOR_NEGATED) each time the timer expires and is accumulated
 * into value every step. */
s32 suzuki_smd_modulator_step_triangle(suzuki_modulator_t* modulator) {
    s32 increment;

    if (--modulator->timer == 0) {
        increment = modulator->amplitude;
        modulator->timer = modulator->timer_reset;
        if ((modulator->flags & SUZUKI_MODULATOR_NEGATED) != 0)
            increment = -increment;
        modulator->increment = increment;
        modulator->flags ^= SUZUKI_MODULATOR_NEGATED;
    }
    modulator->value += modulator->increment;
    return modulator->value;
}
