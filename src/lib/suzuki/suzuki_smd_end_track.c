#include "fft/suzuki.h"
#include "psx/libspu.h"
#include "psx/types.h"

/* SMD opcode 0xFF "End Track": when the envelope of the channel's SPU voice
 * has reached zero, cancels its pending key-on and key-off, releases the
 * channel (flags = 0) and leaves the read position on the opcode. For the
 * SFX record the voice is queued for SFX key-off, dropped from the
 * LFO/noise/reverb voice masks and from g_suzuki_sound_sfx_reserved_voices, and its
 * LFO, noise and reverb marks are set so those voice masks are recalculated;
 * otherwise it is queued for music key-off. While the envelope is non-zero
 * the handler only returns the position after the opcode. */
u8* suzuki_smd_end_track(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s32 key_stat;
    s16 envx;
    u32 mask;

    SpuGetVoiceEnvelopeAttr(channel->voice, &key_stat, &envx);
    if (envx == 0) {
        channel->voice_update_flags &= ~(SUZUKI_VOICE_UPDATE_KEY_ON | SUZUKI_VOICE_UPDATE_KEY_OFF);
        if (music->status & SUZUKI_MUSIC_STATUS_SFX) {
            g_suzuki_sound_sfx_key_off_voices |= channel->voice_attr.voice;
            mask = ~channel->voice_attr.voice;
            music->lfo_voice_mask &= mask;
            music->noise_voice_mask &= mask;
            music->reverb_voice_mask &= mask;
            g_suzuki_sound_sfx_reserved_voices &= mask;
            channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN | SUZUKI_VOICE_ATTR_ATTACK | SUZUKI_VOICE_ATTR_PITCH;
        } else {
            g_suzuki_sound_music_key_off_voices |= channel->voice_attr.voice;
        }
        channel->flags = 0;
        note_data--;
    }
    return note_data;
}
