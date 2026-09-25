#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0x8A: a separate copy of
 * main_smd_no_instruction. */
u8* main_smd_no_instruction_2(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    return note_data;
}
