/* Commit the queued key-ons of playing sequences. */

#include "fft/suzuki.h"

/* Voices reserved or awaiting release by sound effects are allowed only for
 * the SFX music record. The root-counter handler contains the same loop. */
void suzuki_sound_flush_key_on(void) {
    suzuki_music_t* music;
    s16 status;
    u32 allowed;
    u32 voices;

    voices = 0;
    allowed = ~(g_suzuki_sound_sfx_reserved_voices | g_suzuki_sound_sfx_key_off_voices);
    music = g_suzuki_sound_active_music_list;
    while (music != 0) {
        status = music->status;
        if (status < 0) {
            if ((status & SUZUKI_MUSIC_STATUS_SFX) != 0)
                voices |= music->key_on_mask;
            else
                voices |= allowed & music->key_on_mask;
            music->key_on_mask = 0;
        }
        music = music->next;
    }

    if (voices != 0)
        SpuSetKey(SPU_ON, voices);
}
