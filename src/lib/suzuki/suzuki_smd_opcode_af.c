#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xAF "Nop": consumes no parameter. */
u8* suzuki_smd_opcode_af(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    return note_data;
}
