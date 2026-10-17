#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xA9 "Gate Time": how many sixteenths of its length each note
 * stays keyed on (suzuki_smd_read_instructions; 0x10 is the whole length). */
u8* suzuki_smd_gate_time(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->gate_time = *note_data;
    return note_data + 1;
}
