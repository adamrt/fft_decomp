#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xB8 "Acoustics": reverb depth, delay and feedback, passed on
 * with reverb mode 10, for which suzuki_sound_set_reverb_mode only records
 * them. */
u8* suzuki_smd_acoustics(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s16 depth = note_data[0] << 8;
    s8 delay;
    s8 feedback;

    music->reverb_depth = depth;
    delay = note_data[1];
    music->reverb_delay = delay;
    feedback = note_data[2];
    music->reverb_feedback = feedback;
    suzuki_sound_set_reverb_mode(10, depth, delay, feedback);
    return note_data + 3;
}
