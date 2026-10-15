#include "fft/main.h"

/* Cancels a pending tune, or fades a playing one out to volume 0 over time
 * ticks (0: at once). */
int main_sound_set_tune_volume(int time) {
    int tune = g_main_sound_music.slots.tune;

    if (tune != 0) {
        if ((tune & MAIN_SOUND_TUNE_PENDING) != 0) {
            g_main_sound_music.slots.tune = 0;
        } else {
            suzuki_smd_set_music_volume(g_main_sound_music.slots.music[1 + (tune & MAIN_SOUND_TUNE_ID_MASK)], 0, time);
        }
    }
    return 1;
}
