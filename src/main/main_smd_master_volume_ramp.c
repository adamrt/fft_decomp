#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xA7: ramps the MUS master volume to a
 * target byte over 32 steps per unit of the count byte. */
u8* main_smd_master_volume_ramp(u8* note_data, suzuki_music_t* music, void* channel) {
    s16 steps;
    u8 target;
    s32 delta;

    steps = note_data[0] << 5;
    target = note_data[1];
    delta = (target << 24) - music->master_volume.value;
    if (steps != 0 && delta != 0) {
        music->master_volume.count = steps;
        music->master_volume.target = target << 8;
        music->master_volume.step = delta / steps;
    }
    return note_data + 2;
}
