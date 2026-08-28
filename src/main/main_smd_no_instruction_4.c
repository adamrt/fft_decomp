#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0x8F: a separate copy of
 * main_smd_no_instruction. */
u8* main_smd_no_instruction_4(u8* note_data, void* music, void* channel) {
    return note_data;
}
