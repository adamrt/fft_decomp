#include "fft/wldcore.h"
#include "psx/types.h"

/* Queue a music change: fade out what is playing, start `value` (a packed
 * slot and music id) and fade it in over 0x10 steps. */
void wldcore_sound_enqueue_music_start(s32 value) {
    if (g_wldcore_audio_queue.music.flags & WLDCORE_AUDIO_MUSIC_PLAYING) {
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
    }
    wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, value);
    wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 0x10);
}
