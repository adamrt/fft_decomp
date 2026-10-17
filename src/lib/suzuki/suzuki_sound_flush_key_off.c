/* Commit queued key-offs and release stopped music and sound-effect voices. */

#include "fft/suzuki.h"

/* Apply the same SFX reservation filter as key-on. Stopped music and sound
 * effects awaiting release receive a fast envelope release before key-off.
 * The root-counter handler contains the same loop. */
void suzuki_sound_flush_key_off(void) {
    suzuki_music_t* music;
    s16 status;
    u32 allowed;
    u32 voices;
    u32 pending;
    s32 voice;

    allowed = ~(g_suzuki_sound_sfx_reserved_voices | g_suzuki_sound_sfx_key_off_voices);
    voices = (allowed & g_suzuki_sound_music_key_off_voices) | g_suzuki_sound_sfx_key_off_voices;
    music = g_suzuki_sound_active_music_list;
    while (music != 0) {
        status = music->status;
        if (status < 0) {
            if ((status & SUZUKI_MUSIC_STATUS_SFX) != 0)
                voices |= music->key_off_mask;
            else
                voices |= allowed & music->key_off_mask;
            music->key_off_mask = 0;
        }
        music = music->next;
    }

    pending = (allowed & g_suzuki_sound_music_key_off_voices) | g_suzuki_sound_sfx_key_off_voices;
    if (pending != 0) {
        for (voice = 23; voice >= 0; voice--) {
            if ((pending & SPU_VOICECH(voice)) != 0)
                SpuSetVoiceRRAttr(voice, 6, SPU_VOICE_LINEARDecN);
        }
        g_suzuki_sound_sfx_key_off_voices = 0;
        g_suzuki_sound_music_key_off_voices = 0;
    }

    if (voices != 0)
        SpuSetKey(SPU_OFF, voices);
}
