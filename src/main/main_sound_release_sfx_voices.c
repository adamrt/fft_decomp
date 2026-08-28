#include "fft/main_sound.h"
#include "psx/api.h"
#include "psx/etc.h"
#include "psx/types.h"

/* Removes stopped SFX channels from the SFX music's channel mask and their
 * voices from the key-on, pitch-LFO, noise, reverb and reserved-voice masks,
 * queueing the voices for key-off.
 *
 * Both masks are updated in place (`channel_mask &= ...`, `voice_mask |=
 * ...`); separate temporaries allocate differently from the target. */
void main_sound_release_sfx_voices(suzuki_music_t* music, u32 channel_mask, u32 voice_mask) {
    u32 keep_voices;

    channel_mask = ~channel_mask;
    DisableEvent(g_main_root_counter_2_event);
    keep_voices = ~voice_mask;
    channel_mask &= music->channel_mask;
    music->channel_mask = channel_mask;
    voice_mask |= g_main_sound_sfx_key_off_voices;
    g_main_sound_sfx_key_off_voices = voice_mask;
    music->key_on_mask &= keep_voices;
    g_main_mask_exclusion &= keep_voices;
    music->lfo_voice_mask &= keep_voices;
    music->noise_voice_mask &= keep_voices;
    music->reverb_voice_mask &= keep_voices;
    EnableEvent(g_main_root_counter_2_event);
}
