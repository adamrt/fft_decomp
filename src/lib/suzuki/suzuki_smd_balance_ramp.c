#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xEA "Balance Ramp": ramps the balance to a target byte over
 * count ticks. balance_step is the per-tick delta that
 * suzuki_smd_update_channel_ramps adds while SUZUKI_RAMP_BALANCE is set. */
u8* suzuki_smd_balance_ramp(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 count = note_data[0];
    s32 delta = (s8)note_data[1] - (channel->balance >> 8);

    if (count != 0 && delta != 0) {
        channel->balance_step_count = count;
        channel->ramp_flags |= SUZUKI_RAMP_BALANCE;
        channel->balance_step = (delta << 8) / count;
    }
    return note_data + 2;
}
