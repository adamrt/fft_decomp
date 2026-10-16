#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xF1 "Modulator Set Depth": sets the speed and the depth of
 * the modulator selected by 0xF0. Parameters: speed, then a signed 16-bit
 * depth (high byte first) scaled to the high 16 bits of the 32-bit value. */
u8* suzuki_smd_modulator_set_depth(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator = &channel->modulators[channel->modulator_index];
    s16 speed = note_data[0];

    modulator->amplitude = suzuki_smd_modulator_calculate_step(
        ((s8)note_data[1] << 24) | (note_data[2] << 16), speed, modulator->waveform);
    modulator->timer_reset = speed;
    return note_data + 3;
}
