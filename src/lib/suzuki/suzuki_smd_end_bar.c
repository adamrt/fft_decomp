#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0x90 "End Bar": jumps back to the loop point set by Loop
 * (0x91), counting the pass and restoring the octave. Without a loop
 * point the channel ends: its SPU voice is queued for key-off (the SFX MUS
 * record also drops the voice from its LFO, noise and reverb masks and from
 * the SFX reservation, and marks envelope and LFO parameters for update),
 * and the read position stays on this opcode. */
u8* suzuki_smd_end_bar(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 mask;

    if (channel->loop_note_data) {
        note_data = channel->loop_note_data;
        channel->loop_count++;
        channel->octave_base = channel->loop_octave_base;
    } else {
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
