#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xC6 "Sustain Level". */
u8* suzuki_smd_sustain_level(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 level = note_data[0];

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN_LEVEL;
    channel->voice_attr.sl = level;
    return note_data + 1;
}
