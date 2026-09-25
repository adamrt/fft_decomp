#include "fft/main.h"
#include "psx/types.h"

/* Modulator step: triangle wave; the increment flips sign (flags bit 3)
 * each time the timer expires and is accumulated into value every step. */
s32 main_smd_modulator_step_triangle(suzuki_modulator_t* modulator) {
    s32 increment;
    u16 timer;

    timer = modulator->timer - 1;
    modulator->timer = timer;
    if (timer == 0) {
        increment = modulator->amplitude;
        modulator->timer = modulator->timer_reset;
        if ((modulator->flags & 8) != 0)
            increment = -increment;
        modulator->increment = increment;
        modulator->flags ^= 8;
    }
    modulator->value += modulator->increment;
    return modulator->value;
}
