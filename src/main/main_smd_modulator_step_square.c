#include "fft/main.h"
#include "psx/types.h"

/* Modulator step: square wave between 0 and amplitude, toggling each time
 * the timer expires. Returns the current value. */
s32 main_smd_modulator_step_square(suzuki_modulator_t* modulator) {
    s32 next_value = 0;
    u16 timer;

    timer = modulator->timer - 1;
    modulator->timer = timer;
    if (timer == 0) {
        modulator->timer = modulator->timer_reset;
        if (modulator->value == 0)
            next_value = modulator->amplitude;
        modulator->value = next_value;
    }
    return modulator->value;
}
