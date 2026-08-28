#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xE2 "Fermata Ramp": ramps the channel volume to a
 * signed target byte over the given number of steps. */
u8* main_smd_fermata_ramp(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    u16 steps;
    s32 delta;

    steps = note_data[0];
    delta = ((s8)note_data[1] << 24) - channel->volume;
    if (steps != 0 && delta != 0) {
        channel->volume_step_count = steps;
        channel->flags_06 |= 8;
        channel->volume_step = delta / steps;
    }
    return note_data + 2;
}
