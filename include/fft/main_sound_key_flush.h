#ifndef FFT_MAIN_SOUND_KEY_FLUSH_H
#define FFT_MAIN_SOUND_KEY_FLUSH_H

#include "fft/main_sound.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Suzuki key-on/key-off flushes. The driver compiled them both as functions
 * and inline in its root-counter handler, so their bodies live here once. */

/* Keys on the voices queued in each playing MUS record's key_on_mask; voices
 * reserved or pending release by SFX (g_main_mask_exclusion, g_main_sound_sfx_key_off_voices) only
 * for the SFX record (status bit 1). main_sound_flush_key_on (0x80014818) is
 * this body alone; the root-counter handler (0x800149dc) contains it inline. */
static inline void main_sound_flush_key_on_inline(void) {
    suzuki_music_t* music;
    s16 status;
    u32 allowed;
    u32 voices;

    voices = 0;
    allowed = ~(g_main_mask_exclusion | g_main_sound_sfx_key_off_voices);
    music = g_main_sound_active_music_list;
    while (music != 0) {
        status = music->status;
        if (status < 0) {
            if ((status & 2) != 0)
                voices |= music->key_on_mask;
            else
                voices |= allowed & music->key_on_mask;
            music->key_on_mask = 0;
        }
        music = music->next;
    }

    if (voices != 0)
        SpuSetKey(1, voices);
}

/* Keys off the voices queued in each playing MUS record's key_off_mask (same
 * SFX filter as the key-on flush) plus the voices of stopped music
 * (g_main_sound_music_key_off_voices) and the SFX voices pending release (g_main_sound_sfx_key_off_voices), which first
 * get a fast release (rate 6, mode 3). main_sound_flush_key_off (0x800148b4)
 * is this body alone; the root-counter handler contains it inline. */
static inline void main_sound_flush_key_off_inline(void) {
    suzuki_music_t* music;
    s16 status;
    u32 allowed;
    u32 voices;
    u32 pending;
    s32 voice;

    allowed = ~(g_main_mask_exclusion | g_main_sound_sfx_key_off_voices);
    voices = (allowed & g_main_sound_music_key_off_voices) | g_main_sound_sfx_key_off_voices;
    music = g_main_sound_active_music_list;
    while (music != 0) {
        status = music->status;
        if (status < 0) {
            if ((status & 2) != 0)
                voices |= music->key_off_mask;
            else
                voices |= allowed & music->key_off_mask;
            music->key_off_mask = 0;
        }
        music = music->next;
    }

    pending = (allowed & g_main_sound_music_key_off_voices) | g_main_sound_sfx_key_off_voices;
    if (pending != 0) {
        for (voice = 23; voice >= 0; voice--) {
            if ((pending & (1 << voice)) != 0)
                SpuSetVoiceRRAttr(voice, 6, 3);
        }
        g_main_sound_sfx_key_off_voices = 0;
        g_main_sound_music_key_off_voices = 0;
    }

    if (voices != 0)
        SpuSetKey(0, voices);
}

#endif
