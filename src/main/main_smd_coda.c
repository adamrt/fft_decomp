#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0x99 "Coda": ends a repeated section. While passes
 * remain it records this position and octave for To Coda (0x9A) and jumps
 * back to the section start; after the last pass it pops the entry. */
u8* main_smd_coda(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_repeat_t* repeat;

    repeat = &channel->repeats[channel->repeat_depth];
    if (--repeat->count != 0xff) {
        repeat->coda = note_data;
        repeat->coda_octave = channel->octave_base;
        note_data = repeat->start;
        channel->octave_base = repeat->octave_base;
    } else {
        channel->repeat_depth--;
    }
    return note_data;
}
