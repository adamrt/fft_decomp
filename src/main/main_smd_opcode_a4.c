#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xA4: sets the song-wide byte
 * field_1c. */
u8* main_smd_opcode_a4(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->_unknown_01c = *note_data;
    return note_data + 1;
}
