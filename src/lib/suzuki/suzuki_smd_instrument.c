#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xAC "Instrument". */
u8* suzuki_smd_instrument(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_smd_set_instrument(*note_data++, channel);
    return note_data;
}
