#include "fft/main.h"
#include "psx/types.h"

/* Modulator step: sawtooth; value ramps by amplitude every step and resets
 * to zero when the timer expires. */
s32 main_smd_modulator_step_sawtooth(suzuki_modulator_t* modulator) {
    u16 timer;

    timer = modulator->timer - 1;
    modulator->timer = timer;
    if (timer == 0) {
        modulator->value = 0;
        modulator->timer = modulator->timer_reset;
    } else {
        modulator->value += modulator->amplitude;
    }
    return modulator->value;
}
