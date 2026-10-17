#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0x8A "Nop": a separate copy of suzuki_smd_no_instruction, with
 * length 1 in g_suzuki_smd_opcode_lengths. */
u8* suzuki_smd_opcode_8a(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    return note_data;
}
