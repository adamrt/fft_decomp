#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0x91 "Loop": marks the loop point End Bar (0x90) jumps to,
 * saving the note-data position and the current octave base. */
u8* suzuki_smd_loop(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->loop_note_data = note_data;
    channel->loop_octave_base = channel->octave_base;
    return note_data;
}
