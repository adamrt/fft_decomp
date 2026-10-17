#include "fft/suzuki.h"

/* Stops every SMD music record and silences every SFX channel. */
void suzuki_sound_stop_all(void) {
    suzuki_smd_stop_marked_music();
    suzuki_sound_stop_all_sfx();
}
