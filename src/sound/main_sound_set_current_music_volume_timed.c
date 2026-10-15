#include "fft/main.h"
#include "psx/types.h"

/* Fades the current music back to the remembered volume over time ticks. */
int main_sound_set_current_music_volume_timed(int time) {
    if (g_main_sound_music.state.current_music != 0) {
        suzuki_smd_set_music_volume(g_main_sound_music.state.current_music, g_main_sound_music.state.volume, time);
        return 1;
    }
    return 0;
}
