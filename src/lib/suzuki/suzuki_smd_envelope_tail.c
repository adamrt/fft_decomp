#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xC7 "Envelope Tail": decay rate then sustain level. */
u8* suzuki_smd_envelope_tail(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 level;

    channel->voice_attr.dr = note_data[0];
    level = note_data[1];
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN_LEVEL | SUZUKI_VOICE_ATTR_DECAY;
    channel->voice_attr.sl = level;
    return note_data + 2;
}
