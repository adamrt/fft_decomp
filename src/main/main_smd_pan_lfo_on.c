#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xEE: activates modulator 2. */
u8* main_smd_pan_lfo_on(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[2].flags |= 1;
    return note_data;
}
