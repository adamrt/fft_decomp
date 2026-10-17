#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xC4 "Sustain Rate". */
u8* suzuki_smd_sustain_time(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 value = *note_data++;

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN;
    channel->voice_attr.sr = value;
    return note_data;
}
