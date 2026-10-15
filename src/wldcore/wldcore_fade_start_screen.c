#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_fade_start_screen(s32 flags, s32 duration) {
    if ((g_wldcore_screen_fade_state.flags[0] & WLDCORE_SCREEN_FADE_OUT) != (flags & WLDCORE_SCREEN_FADE_OUT)) {
        g_wldcore_screen_fade_state.flags[0] = flags | WLDCORE_SCREEN_FADE_RUNNING;
        g_wldcore_screen_fade_state.elapsed = 0;
        g_wldcore_screen_fade_state.duration = duration;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_FADING;
        if (flags & WLDCORE_SCREEN_FADE_START_MUSIC) {
            wldcore_sound_enqueue_audio_command(
                WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 0x10);
        }
        if (flags & WLDCORE_SCREEN_FADE_STOP_MUSIC) {
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 4);
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_UNLOAD, 2);
        }
    }
}
