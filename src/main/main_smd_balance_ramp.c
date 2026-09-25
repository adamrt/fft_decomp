#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xEA: ramps the balance to a target byte
 * over count ticks. balance_step is the per-tick delta; flags_06 bit 4 lets
 * the channel ramp pass (0x80015138) step it. */
u8* main_smd_balance_ramp(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 count;
    s32 delta;

    count = note_data[0];
    delta = (s8)note_data[1] - (channel->balance >> 8);
    if (count != 0 && delta != 0) {
        channel->balance_step_count = count;
        channel->flags_06 |= 0x10;
        channel->balance_step = (delta << 8) / count;
    }
    return note_data + 2;
}
