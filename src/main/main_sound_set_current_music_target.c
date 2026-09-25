#include "fft/main_sound.h"
#include "psx/types.h"

int main_sound_set_current_music_target(int volume, int time) {
    suzuki_music_t* music = g_main_sound_music.state.current_music;

    if (music != 0) {
        g_main_sound_music.state.volume = volume;
        SuzukiCalcMusVolChange(music, (s16)volume, (s16)time);
        return 1;
    }
    return 0;
}
