#include "fft/main.h"

/* Requests a tune (main_sound_tune_e) unless one is pending or playing: the
 * scenario music fades out and main_sound_update_tunes starts the tune once
 * it has stopped. */
int main_sound_play_tune(int tune) {
    if (tune != 0) {
        if (g_main_sound_music.slots.tune == 0) {
            main_sound_set_current_music_target(0, 120);
            g_main_sound_music.slots.tune = tune | MAIN_SOUND_TUNE_PENDING;
        }
    }
    return 1;
}
