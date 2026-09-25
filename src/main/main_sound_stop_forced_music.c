#include "fft/main.h"
#include "psx/types.h"

s32 main_sound_stop_forced_music(void) {
    suzuki_music_t** music = &g_main_sound_music.state.current_music;
    s32 result = 0;

    if (*music != 0) {
        SuzukiDeallocateMUSChannels(*music);
        result = 1;
        g_main_sound_music.state.scenario_track = 0;
        g_main_sound_music.state.forced_music = 0;
        *music = 0;
    }
    return result;
}
