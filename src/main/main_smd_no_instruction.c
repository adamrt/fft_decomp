#include "fft/main_sound.h"
#include "psx/types.h"

/* Handler of the 37 unassigned SMD opcode slots: consumes nothing. */
u8* main_smd_no_instruction(u8* note_data, void* music, void* channel) {
    return note_data;
}
