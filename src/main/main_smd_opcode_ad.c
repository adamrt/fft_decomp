#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xAD: a non-zero byte is added to
 * length_adjust, a zero byte resets it. */
u8* main_smd_opcode_ad(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 delta;

    delta = *note_data++;
    if (delta != 0)
        channel->length_adjust += delta;
    else
        channel->length_adjust = 0;
    return note_data;
}
