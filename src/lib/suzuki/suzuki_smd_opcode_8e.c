#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0x8E "Skip 3": ignores its three parameter bytes. */
u8* suzuki_smd_opcode_8e(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    return note_data + 3;
}
