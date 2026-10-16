#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xE9 "Shift Balance": signed balance delta. The byte is
 * fetched unsigned and sign-extended afterwards, as in "Balance". */
u8* suzuki_smd_shift_balance(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s8 delta = note_data[0];

    channel->balance = (channel->balance + (delta << 8)) & 0x7fff;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
    return note_data + 1;
}
