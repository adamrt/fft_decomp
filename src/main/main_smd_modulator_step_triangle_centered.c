#include "fft/main.h"
#include "psx/types.h"

/* Modulator step: triangle wave centred on its start value. The first
 * half-period lasts timer_reset steps; flags bit 2 then doubles every later
 * one, so value swings between -amplitude * timer_reset and
 * +amplitude * timer_reset. The increment flips sign (flags bit 3) each time
 * the timer expires. */
s32 main_smd_modulator_step_triangle_centered(suzuki_modulator_t* modulator) {
    s32 amplitude;
    u16 flags;
    u16 timer;

    flags = modulator->flags;
    timer = modulator->timer - 1;
    modulator->timer = timer;
    if (timer == 0) {
        modulator->timer = modulator->timer_reset;
        modulator->timer *= (flags & 4) ? 2 : 1;
        amplitude = modulator->amplitude;
        modulator->increment = amplitude;
        if ((flags & 8) != 0)
            modulator->increment = -amplitude;
        flags = (flags | 4) ^ 8;
        modulator->flags = flags;
    }
    modulator->value += modulator->increment;
    return modulator->value;
}
