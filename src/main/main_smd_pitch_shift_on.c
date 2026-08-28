#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xDA: activates modulator 0. */
u8* main_smd_pitch_shift_on(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    channel->modulators[0].flags |= 1;
    return note_data;
}
