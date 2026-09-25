#include "fft/main.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Collects the noise voice masks of every playing MUS record; records with
 * status bit 0 lose the voices reserved by SFX (g_main_mask_exclusion). */
void main_sound_calculate_noise_voices(void) {
    suzuki_music_t* music;
    s32 status;
    u32 excludable_voices;
    u32 forced_voices;

    forced_voices = 0;
    excludable_voices = 0;
    music = g_main_sound_active_music_list;
    while (music != 0) {
        /* The target tests the playing bit (15) as a sign: `lh` and `bgez`. */
        status = (s16)music->status;
        if (status < 0) {
            if ((status & 1) != 0)
                excludable_voices |= music->noise_voice_mask;
            else
                forced_voices |= music->noise_voice_mask;
        }
        music = music->next;
    }

    excludable_voices = (excludable_voices & ~g_main_mask_exclusion) | forced_voices;
    SpuSetNoiseVoice(1, excludable_voices);
    SpuSetNoiseVoice(0, ~excludable_voices);
}
