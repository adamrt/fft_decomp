#include "fft/main.h"
#include "psx/types.h"

/* Modulator step: bipolar sample and hold. Each time the timer expires value
 * takes a new random level between -amplitude and about +amplitude. */
s32 main_smd_modulator_step_random_bipolar(suzuki_modulator_t* modulator) {
    u16 timer;

    timer = modulator->timer - 1;
    modulator->timer = timer;
    if (timer == 0) {
        modulator->timer = modulator->timer_reset;
        modulator->value = (modulator->amplitude >> 14) * main_smd_random() - modulator->amplitude;
    }
    return modulator->value;
}
