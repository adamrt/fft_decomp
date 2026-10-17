#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xC8 "Attack Mode". */
u8* suzuki_smd_attack_mode(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 mode = note_data[0];

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ATTACK;
    channel->voice_attr.a_mode = mode;
    return note_data + 1;
}
