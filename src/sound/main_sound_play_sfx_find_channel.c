#include "fft/main.h"

/* Plays a sound on a free SFX channel pair, or on the oldest stealable one. */
void main_sound_play_sfx_find_channel(int sound_id) {
    suzuki_sound_play_sfx(sound_id);
}
