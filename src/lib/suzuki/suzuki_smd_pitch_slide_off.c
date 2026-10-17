#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xDC "Pitch Slide Off": stops a pitch slide where it is. */
u8* suzuki_smd_pitch_slide_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->ramp_flags &= ~SUZUKI_RAMP_PITCH_SLIDE;
    return note_data;
}
