#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xC0 "Naturale": reloads the channel's current instrument,
 * undoing the envelope opcodes since it was set. */
u8* suzuki_smd_naturale(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_smd_set_instrument(channel->instrument, channel);
    return note_data;
}
