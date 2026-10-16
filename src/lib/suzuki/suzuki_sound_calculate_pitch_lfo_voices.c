#include "psx/libspu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Collects the pitch-LFO voice masks of every playing MUS record and hands
 * them to the SPU; SMD music records lose the voices reserved by SFX
 * (g_suzuki_sound_sfx_reserved_voices), the SFX record keeps its own. */
void suzuki_sound_calculate_pitch_lfo_voices(void) {
    suzuki_music_t* music;
    s32 status;
    u32 excludable_voices;
    u32 forced_voices = 0;

    excludable_voices = 0;
    music = g_suzuki_sound_active_music_list;
    while (music != 0) {
        /* The playing bit (15) is the sign. */
        status = music->status;
        if (status < 0) {
            if ((status & SUZUKI_MUSIC_STATUS_SMD) != 0)
                excludable_voices |= music->lfo_voice_mask;
            else
                forced_voices |= music->lfo_voice_mask;
        }
        music = music->next;
    }

    excludable_voices = (excludable_voices & ~g_suzuki_sound_sfx_reserved_voices) | forced_voices;
    SpuSetPitchLFOVoice(SPU_ON, excludable_voices);
    SpuSetPitchLFOVoice(SPU_OFF, ~excludable_voices);
}
