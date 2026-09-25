#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0x9A "To Coda": on the last pass of a repeated section
 * jumps past its Coda (0x99), restoring the octave recorded there, and pops
 * the entry. */
u8* main_smd_to_coda(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_repeat_t* repeat;

    repeat = &channel->repeats[channel->repeat_depth];
    if (repeat->count == 0) {
        note_data = repeat->coda;
        channel->octave_base = repeat->coda_octave;
        channel->repeat_depth--;
    }
    return note_data;
}
