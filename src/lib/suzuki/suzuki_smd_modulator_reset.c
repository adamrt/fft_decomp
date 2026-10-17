#include "fft/suzuki.h"

/* Restarts a modulator: called by the SMD opcodes that program one (the
 * vibrato, tremolo and pan LFO opcodes) or enable one (0xF5, 0xF6). */
void suzuki_smd_modulator_reset(suzuki_modulator_t* modulator) {
    modulator->timer = 1;
    modulator->value = 0;
    modulator->flags &= ~(SUZUKI_MODULATOR_NEGATED | SUZUKI_MODULATOR_HALF_PERIOD_DONE);
    modulator->delay = modulator->delay_reset;
    modulator->fade_level = modulator->fade_step;
}
