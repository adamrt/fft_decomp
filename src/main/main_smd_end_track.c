#include "fft/main.h"
#include "psx/spu.h"
#include "psx/types.h"

/* SMD opcode 0xFF: when the envelope of the channel's
 * SPU voice has reached zero, clears note_flags2 bits 0-1, releases the
 * channel (active = 0) and leaves the read position on the opcode. For the
 * SFX record (status bit 1) the voice is queued for SFX key-off, dropped from
 * the LFO/noise/reverb voice masks and from g_main_mask_exclusion, and its
 * attack, sustain and LFO parameters are marked for update (func_flags
 * 0x54); otherwise it is queued for music key-off. While the envelope is
 * non-zero the handler only returns the position after the opcode. */
u8* main_smd_end_track(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s32 key_stat;
    s16 envx;
    u32 mask;

    SpuGetVoiceEnvelopeAttr(channel->voice, &key_stat, &envx);
    if (envx == 0) {
        channel->note_flags2 &= ~3;
        if (music->status & 2) {
            g_main_sound_sfx_key_off_voices |= channel->voice_mask;
            mask = ~channel->voice_mask;
            music->lfo_voice_mask &= mask;
            music->noise_voice_mask &= mask;
            music->reverb_voice_mask &= mask;
            g_main_mask_exclusion &= mask;
            channel->func_flags |= 0x54;
        } else {
            g_main_sound_music_key_off_voices |= channel->voice_mask;
        }
        channel->active = 0;
        note_data--;
    }
    return note_data;
}
