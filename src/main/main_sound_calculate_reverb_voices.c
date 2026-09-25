#include "fft/main.h"
#include "psx/types.h"

/* Sibling of the pitch-LFO (0x80014f58) and noise (0x80014ff8) voice
 * calculators; the three voice masks feed the SPU pitch-LFO / noise / reverb
 * voice registers. */
void main_sound_calculate_reverb_voices(void) {
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
                excludable_voices |= music->reverb_voice_mask;
            else
                forced_voices |= music->reverb_voice_mask;
        }
        music = music->next;
    }

    excludable_voices = (excludable_voices & ~g_main_mask_exclusion) | forced_voices;
    SpuSetReverbVoice(1, excludable_voices);
    SpuSetReverbVoice(0, ~excludable_voices);
}
