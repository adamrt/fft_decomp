#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xB1: clears bit 11 of the channel flag word. */
u8* main_smd_opcode_b1(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->active &= ~0x800;
    return note_data;
}
