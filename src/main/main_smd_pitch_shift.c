#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xD8 "Pitch Shift": programs modulator 0 (pitch) as a
 * centred triangle (waveform 3) with the given speed, signed depth and
 * delay, and restarts it. The depth is squared with its sign kept.
 *
 * The fields are stored through channel: unlike its twins, a modulator
 * pointer here gets its own base register, which the target does not use. */
u8* main_smd_pitch_shift(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    s32 depth;
    s16 speed;

    depth = (s8)note_data[1];
    speed = note_data[0];
    if (depth != 0 && speed != 0) {
        if (depth < 0) {
            depth = depth * -depth;
        } else {
            depth = depth * depth;
        }
        channel->modulators[0].amplitude = main_smd_modulator_calculate_step(depth << 14, speed, 3);
        channel->modulators[0].timer_reset = speed;
        channel->modulators[0].counter_14_reset = note_data[2];
        channel->modulators[0].counter_18_reset = 0x100;
        channel->modulators[0].step = main_smd_modulator_step_triangle_centered;
        channel->modulators[0].waveform = 3;
        channel->modulators[0].target = 0;
        channel->modulators[0].flags = 3;
        main_smd_modulator_reset(&channel->modulators[0]);
    }
    return note_data + 3;
}
