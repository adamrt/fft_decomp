#include "fft/main_sound.h"
#include "psx/types.h"

/* Deactivates a modulator. */
void main_smd_modulator_deactivate(suzuki_modulator_t* modulator) {
    modulator->flags &= ~1;
}
