#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xC0 "Naturale": reprograms the channel with its own
 * current instrument, cancelling any transposition. */
u8* main_smd_naturale(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    main_smd_set_instrument(channel->instrument, channel);
    return note_data;
}
