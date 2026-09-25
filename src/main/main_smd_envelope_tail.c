#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xC7 "Envelope Tail": decay time then sustain level. */
u8* main_smd_envelope_tail(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 level;

    channel->decay_time = note_data[0];
    level = note_data[1];
    channel->func_flags |= 0x120;
    channel->sustain_level = level;
    return note_data + 2;
}
