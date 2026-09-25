#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xAF: consumes no parameter. */
u8* main_smd_no_instruction_6(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    return note_data;
}
