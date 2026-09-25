#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0x9D: little-endian channel mask. */
u8* main_smd_stop_sfx(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    main_sound_stop_sfx_channels(note_data[0] | (note_data[1] << 8));
    return note_data + 2;
}
