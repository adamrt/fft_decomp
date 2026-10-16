#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xCA "Release Mode". */
u8* suzuki_smd_release_mode(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 mode = note_data[0];

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_RELEASE;
    channel->voice_attr.r_mode = mode;
    return note_data + 1;
}
