#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_sound_enqueue_music_start(s32 value) {
    if (g_wldcore_audio_queue.music.flags & 1) {
        wldcore_sound_enqueue_audio_command(2, 0x10);
    }
    wldcore_sound_enqueue_audio_command(1, value);
    wldcore_sound_enqueue_audio_command(3, 0x10);
}
