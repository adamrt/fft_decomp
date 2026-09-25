#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xD5: toggles bit 1 of flags_06. */
u8* main_smd_opcode_d5(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->flags_06 ^= 2;
    return note_data;
}
