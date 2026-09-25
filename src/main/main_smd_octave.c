#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0x94 "Octave": twelve semitones per octave. */
u8* main_smd_octave(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->octave_base = note_data[0] * 12;
    return note_data + 1;
}
