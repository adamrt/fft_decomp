#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0x8D: when the parameter byte matches the
 * MUS channel selector, remember the position and octave for a later jump. */
u8* main_smd_opcode_8d(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 selector;

    selector = *note_data++;
    if (selector == music->channel_select) {
        channel->loop_note_data = note_data;
        channel->loop_octave_base = channel->octave_base;
    }
    return note_data;
}
