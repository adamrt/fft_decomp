#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xD4 "Pitch Slide": slides the pitch by a signed number of
 * semitones over the given number of ticks; a zero count or amount stops the
 * slide. */
u8* suzuki_smd_pitch_slide(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 steps = note_data[0];
    s32 amount = (s8)note_data[1] << 24;

    if (steps != 0 && amount != 0) {
        channel->pitch_slide_count = steps;
        channel->ramp_flags |= SUZUKI_RAMP_PITCH_SLIDE;
        channel->pitch_slide_step = amount / steps;
    } else {
        channel->ramp_flags &= ~SUZUKI_RAMP_PITCH_SLIDE;
    }
    return note_data + 2;
}
