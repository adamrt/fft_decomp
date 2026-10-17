#include "fft/suzuki.h"
#include "psx/types.h"

/* Modulator step: bipolar sample and hold. Each time the timer expires value
 * takes a new random level between -amplitude and about +amplitude. */
s32 suzuki_smd_modulator_step_random_bipolar(suzuki_modulator_t* modulator) {
    if (--modulator->timer == 0) {
        modulator->timer = modulator->timer_reset;
        modulator->value = (modulator->amplitude >> 14) * suzuki_smd_random() - modulator->amplitude;
    }
    return modulator->value;
}
