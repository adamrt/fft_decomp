#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xAC "Instrument". */
u8* main_smd_instrument(u8* note_data, void* music, void* channel) {
    main_smd_set_instrument(*note_data++, channel);
    return note_data;
}
