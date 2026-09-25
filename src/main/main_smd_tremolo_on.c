#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xE6: activates modulator 1. */
u8* main_smd_tremolo_on(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[1].flags |= 1;
    return note_data;
}
