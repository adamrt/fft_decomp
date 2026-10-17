#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xA7 "Master Volume Ramp": ramps the MUS master volume to a
 * target byte over 32 root-counter ticks per unit of the count byte. */
u8* suzuki_smd_master_volume_ramp(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s16 steps = note_data[0] << 5;
    u8 target = note_data[1];
    s32 delta = (target << 24) - music->master_volume.value;

    if (steps != 0 && delta != 0) {
        music->master_volume.count = steps;
        music->master_volume.target = target << 8;
        music->master_volume.step = delta / steps;
    }
    return note_data + 2;
}
