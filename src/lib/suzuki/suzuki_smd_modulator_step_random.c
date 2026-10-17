#include "fft/suzuki.h"
#include "psx/types.h"

/* Modulator step: sample and hold. Each time the timer expires value takes a
 * new random multiple of amplitude >> 15 (0 to about amplitude). The generator
 * is also advanced on every step. */
s32 suzuki_smd_modulator_step_random(suzuki_modulator_t* modulator) {
    suzuki_smd_random();
    if (--modulator->timer == 0) {
        modulator->timer = modulator->timer_reset;
        modulator->value = (modulator->amplitude >> 15) * suzuki_smd_random();
    }
    return modulator->value;
}
