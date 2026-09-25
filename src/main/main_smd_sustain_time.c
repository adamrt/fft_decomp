#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xC4 "Sustain". */
u8* main_smd_sustain_time(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 value;

    value = *note_data++;
    channel->func_flags |= 0x40;
    channel->sustain_time = value;
    return note_data;
}
