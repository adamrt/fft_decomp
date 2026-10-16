#include "psx/suzuki.h"
#include "psx/types.h"

/* Modulator step: triangle wave centered on its start value. The first
 * half-period lasts timer_reset steps; SUZUKI_MODULATOR_HALF_PERIOD_DONE
 * then doubles every later one, so value swings between
 * -amplitude * timer_reset and +amplitude * timer_reset. The increment flips
 * sign (SUZUKI_MODULATOR_NEGATED) each time the timer expires. */
s32 suzuki_smd_modulator_step_triangle_centered(suzuki_modulator_t* modulator) {
    s32 amplitude;
    u16 flags;

    flags = modulator->flags;
    if (--modulator->timer == 0) {
        modulator->timer = modulator->timer_reset;
        modulator->timer *= (flags & SUZUKI_MODULATOR_HALF_PERIOD_DONE) ? 2 : 1;
        amplitude = modulator->amplitude;
        modulator->increment = amplitude;
        if ((flags & SUZUKI_MODULATOR_NEGATED) != 0)
            modulator->increment = -amplitude;
        flags = (flags | SUZUKI_MODULATOR_HALF_PERIOD_DONE) ^ SUZUKI_MODULATOR_NEGATED;
        modulator->flags = flags;
    }
    modulator->value += modulator->increment;
    return modulator->value;
}
