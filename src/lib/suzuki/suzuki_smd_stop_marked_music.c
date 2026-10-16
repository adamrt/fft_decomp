#include "psx/suzuki.h"

/* Stops every queued record that plays SMD music (not the SFX record),
 * queueing the voices of its active channels for key-off. */
void suzuki_smd_stop_marked_music(void) {
    suzuki_music_t* music = g_suzuki_sound_active_music_list;

    while (music != 0) {
        if (music->status & SUZUKI_MUSIC_STATUS_SMD) {
            music->status &= ~SUZUKI_MUSIC_STATUS_PLAYING;
            g_suzuki_sound_music_key_off_voices |= suzuki_smd_get_active_voice_mask(music);
        }
        music = music->next;
    }
}
