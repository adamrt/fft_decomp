#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0x9D: stop the sound effect with the little-endian sound id.
 * It consumes two parameter bytes; g_suzuki_smd_opcode_lengths gives it three. */
u8* suzuki_smd_stop_sfx(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_sound_stop_sfx_channels(note_data[0] | (note_data[1] << 8));
    return note_data + 2;
}
