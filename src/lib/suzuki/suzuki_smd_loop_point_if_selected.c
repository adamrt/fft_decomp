#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0x8D "Loop Select": sets the loop point like Loop (0x91), but
 * only when its byte equals music->loop_select. */
u8* suzuki_smd_loop_point_if_selected(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 selector = *note_data++;

    if (selector == music->loop_select) {
        channel->loop_note_data = note_data;
        channel->loop_octave_base = channel->octave_base;
    }
    return note_data;
}
