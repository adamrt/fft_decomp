#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xB0: sets bit 11 of the channel flag word. */
u8* main_smd_opcode_b0(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    channel->active |= 0x800;
    return note_data;
}
