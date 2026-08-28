#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xE8 "Balance". */
u8* main_smd_balance(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    channel->balance = note_data[0] << 8;
    channel->note_flags2 |= 0x100;
    return note_data + 1;
}
