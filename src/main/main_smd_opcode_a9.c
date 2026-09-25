#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xA9: one byte parameter stored in
 * gate_time. */
u8* main_smd_opcode_a9(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->gate_time = *note_data;
    return note_data + 1;
}
