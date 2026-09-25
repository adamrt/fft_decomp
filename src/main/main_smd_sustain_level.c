#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xC6 "Sustain Level". */
u8* main_smd_sustain_level(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 level;

    level = note_data[0];
    channel->func_flags |= 0x100;
    channel->sustain_level = level;
    return note_data + 1;
}
