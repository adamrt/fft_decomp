#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xD5 "Pitch Slide Hold": toggles the hold that keeps a pitch
 * slide going past its count. */
u8* suzuki_smd_toggle_pitch_slide_hold(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->ramp_flags ^= SUZUKI_RAMP_PITCH_SLIDE_HOLD;
    return note_data;
}
