#include "fft/main.h"

int main_sound_set_current_music_tempo_scale_timed(int value, int time) {
    if (g_main_sound_music.state.current_music != 0) {
        suzuki_smd_set_tempo_scale(g_main_sound_music.state.current_music, value, time);
        return 1;
    }
    return 0;
}
