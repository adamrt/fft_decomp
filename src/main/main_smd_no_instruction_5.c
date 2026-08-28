#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xAE: consumes no parameter. */
u8* main_smd_no_instruction_5(u8* note_data, void* music, void* channel) {
    return note_data;
}
