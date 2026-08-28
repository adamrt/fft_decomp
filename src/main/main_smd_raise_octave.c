#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0x95 "Raise Octave": raises the octave base by 12
 * semitones. */
u8* main_smd_raise_octave(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    channel->octave_base += 12;
    return note_data;
}
