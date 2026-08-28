#include "fft/main_sound.h"
#include "psx/types.h"

/* Modulator step: square wave between +amplitude and -amplitude, the sign
 * tracked in flags bit 3. Returns the current value. */
s32 main_smd_modulator_step_alternating(suzuki_modulator_t* modulator) {
    s32 next_value;
    u16 timer;

    timer = modulator->timer - 1;
    modulator->timer = timer;
    if (timer == 0) {
        next_value = modulator->amplitude;
        modulator->timer = modulator->timer_reset;
        if ((modulator->flags & 8) != 0)
            next_value = -next_value;
        modulator->value = next_value;
        modulator->flags ^= 8;
    }
    return modulator->value;
}
