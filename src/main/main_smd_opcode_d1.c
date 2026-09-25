#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xD1: add to the channel pitch offset in
 * 1/32 semitone units. Coarse twin of main_smd_opcode_d2. */
u8* main_smd_opcode_d1(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->pitch_offset += (s8)note_data[0] << 5;
    channel->note_flags2 |= 0x200;
    return note_data + 1;
}
