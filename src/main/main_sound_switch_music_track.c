#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "psx/types.h"

s32 main_sound_switch_music_track(s32 track, s32 volume, s32 fade) {
    main_sound_stop_forced_music();
    if (track != 0) {
        if (g_main_sound_music.slots.smd[track] != 0) {
            g_main_sound_music.state.scenario_track = track;
            g_main_sound_music.state.forced_music = g_main_sound_music.slots.smd[track];
            g_main_sound_music.state.current_music = (suzuki_music_t*)g_main_sound_music.slots.handles[track];
            g_main_sound_music.state.volume = volume;
            main_smd_reset_music(g_main_sound_music.state.current_music, (s16)volume, (s16)fade);
        }
        return (s32)g_main_sound_music.state.current_music;
    }
    return 0;
}
