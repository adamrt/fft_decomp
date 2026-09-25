#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xE9 "Shift Balance": signed balance delta. The byte is
 * fetched unsigned and sign-extended afterwards, as in "Balance". */
u8* main_smd_shift_balance(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 delta;

    delta = note_data[0];
    channel->balance = (channel->balance + ((s8)delta << 8)) & 0x7fff;
    channel->note_flags2 |= 0x100;
    return note_data + 1;
}
