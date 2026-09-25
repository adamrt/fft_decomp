#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0x8E: ignores its three
 * parameter bytes. g_main_smd_opcode_lengths also gives it three. */
u8* main_smd_skip_3(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    return note_data + 3;
}
