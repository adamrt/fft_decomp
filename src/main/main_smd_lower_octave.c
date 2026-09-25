#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0x96 "Lower Octave": lowers the octave base by 12
 * semitones. */
u8* main_smd_lower_octave(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->octave_base -= 12;
    return note_data;
}
