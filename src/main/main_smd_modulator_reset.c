#include "fft/main_sound.h"
#include "psx/types.h"

/* Restarts a modulator: called by the SMD opcodes that program one
 * (0xD8 "Pitch Shift", 0xD9, 0xE4, 0xE5, 0xED, 0xF5, 0xF6). */
void main_smd_modulator_reset(suzuki_modulator_t* modulator) {
    modulator->timer = 1;
    modulator->value = 0;
    modulator->flags &= ~0x0c;
    modulator->counter_14 = modulator->counter_14_reset;
    modulator->counter_18 = modulator->counter_18_reset;
}
