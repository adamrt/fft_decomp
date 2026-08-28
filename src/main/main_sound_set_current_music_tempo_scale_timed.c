#include "fft/main_sound.h"
#include "psx/types.h"

int main_sound_set_current_music_tempo_scale_timed(int value, int time) {
    void* music = g_main_sound_music.state.current_music;

    if (music != 0) {
        main_smd_set_tempo_scale(music, (s16)value, (s16)time);
        return 1;
    }
    return 0;
}
