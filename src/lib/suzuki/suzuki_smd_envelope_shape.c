#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xC1 "Envelope Shape": attack, sustain and release
 * modes. */
u8* suzuki_smd_envelope_shape(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 release_mode;

    channel->voice_attr.a_mode = note_data[0];
    channel->voice_attr.s_mode = note_data[1];
    release_mode = note_data[2];
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN_LEVEL | SUZUKI_VOICE_ATTR_RELEASE | SUZUKI_VOICE_ATTR_SUSTAIN
        | SUZUKI_VOICE_ATTR_DECAY | SUZUKI_VOICE_ATTR_ATTACK;
    channel->voice_attr.r_mode = release_mode;
    return note_data + 3;
}
