#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xA5: adds a delta to the song-wide byte
 * that opcode 0xA4 sets. */
u8* main_smd_opcode_a5(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->field_1c += *note_data;
    return note_data + 1;
}
