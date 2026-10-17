#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xAD "Length Adjust": a non-zero byte is added to
 * length_adjust (signed, as the member is s8), a zero byte resets it. */
u8* suzuki_smd_length_adjust(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 delta = *note_data++;

    if (delta != 0)
        channel->length_adjust += delta;
    else
        channel->length_adjust = 0;
    return note_data;
}
