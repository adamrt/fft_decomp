#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xD4: slides the pitch by a signed
 * amount over the given number of steps; a zero count or amount stops the
 * slide. */
u8* main_smd_pitch_slide(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 steps;
    s32 amount;

    steps = note_data[0];
    amount = (s8)note_data[1] << 24;
    if (steps != 0 && amount != 0) {
        channel->pitch_slide_count = steps;
        channel->flags_06 |= 1;
        channel->pitch_slide_step = amount / steps;
    } else {
        channel->flags_06 &= ~1;
    }
    return note_data + 2;
}
