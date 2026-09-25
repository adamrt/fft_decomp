#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_fade_start_screen(s32 flags, s32 duration) {
    if ((g_wldcore_screen_fade_state.flags[0] & 2) != (flags & 2)) {
        g_wldcore_screen_fade_state.flags[0] = flags | 1;
        g_wldcore_screen_fade_state.elapsed = 0;
        g_wldcore_screen_fade_state.duration = duration;
        g_main_system_flags |= 8;
        if (flags & 0x20) {
            wldcore_sound_enqueue_audio_command(1, 0x11b);
            wldcore_sound_enqueue_audio_command(3, 0x10);
        }
        if (flags & 0x10) {
            wldcore_sound_enqueue_audio_command(2, 4);
            wldcore_sound_enqueue_audio_command(4, 2);
        }
    }
}
