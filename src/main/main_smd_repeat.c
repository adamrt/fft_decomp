#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0x98 "Repeat": pushes a repeat entry that plays the
 * following section as many times as its byte gives, remembering the start
 * and the octave. */
u8* main_smd_repeat(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    suzuki_repeat_t* repeat;

    channel->repeat_depth++;
    repeat = &channel->repeats[channel->repeat_depth];
    repeat->count = *note_data++ - 1;
    repeat->start = note_data;
    repeat->octave_base = channel->octave_base;
    return note_data;
}
