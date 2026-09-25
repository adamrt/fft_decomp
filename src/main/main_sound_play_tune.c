#include "fft/main.h"

int main_sound_play_tune(int tune) {
    if (tune != 0) {
        int* current_tune = &g_main_sound_music.slots.tune;

        if (*current_tune == 0) {
            main_sound_set_current_music_target(0, 120);
            *current_tune = tune | 0x80;
        }
    }
    return 1;
}
