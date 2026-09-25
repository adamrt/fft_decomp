#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xD3: adds a signed 16-bit pitch delta
 * carried as a signed high byte followed by an unsigned low byte. */
u8* main_smd_opcode_d3(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 high;
    u8 low;

    high = note_data[0];
    low = note_data[1];
    channel->pitch_offset += low + ((s8)high << 8);
    channel->note_flags2 |= 0x200;
    return note_data + 2;
}
