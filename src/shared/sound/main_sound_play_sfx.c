#include "fft/main.h"

/* Plays a sound on SFX channels 6-7 at the highest priority, replacing
 * whatever those channels play. */
void main_sound_play_sfx(int sound_id) {
    suzuki_sound_play_direct_sfx(sound_id);
}
