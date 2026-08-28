#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xDC: clears bit 0 of flags_06. */
u8* main_smd_opcode_dc(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    channel->flags_06 &= ~1;
    return note_data;
}
