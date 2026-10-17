#include "fft/suzuki.h"

/* Stops a music record: clears its playing bit and queues the voices of its
 * channels for key-off. The record stays queued and loaded. */
void suzuki_smd_stop_music(suzuki_music_t* music) {
    if (music != 0) {
        music->status &= ~SUZUKI_MUSIC_STATUS_PLAYING;
        g_suzuki_sound_music_key_off_voices |= suzuki_smd_get_active_voice_mask(music);
    }
}
