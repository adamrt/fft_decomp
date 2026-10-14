#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xE8 "Balance": 0 is left, 0x40 the center, 0x7f right. */
u8* suzuki_smd_balance(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->balance = note_data[0] << 8;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
    return note_data + 1;
}
