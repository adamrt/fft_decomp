#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xD8 "Vibrato": programs modulator 0 (pitch) as a
 * centered triangle (waveform 3) with the given speed, signed depth and
 * delay, and restarts it. The depth is squared with its sign kept.
 *
 * The fields are stored through channel: unlike its twins, a modulator
 * pointer here gets its own base register, which the target does not use. */
u8* suzuki_smd_pitch_shift(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    const suzuki_smd_modulator_parameters_t* parameters = (const suzuki_smd_modulator_parameters_t*)note_data;
    s32 depth;
    s16 speed;

    depth = parameters->depth;
    speed = parameters->speed;
    if (depth != 0 && speed != 0) {
        if (depth < 0) {
            depth = depth * -depth;
        } else {
            depth = depth * depth;
        }
        channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].amplitude
            = suzuki_smd_modulator_calculate_step(depth << 14, speed, SUZUKI_MODULATOR_WAVEFORM_TRIANGLE_CENTERED);
        channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].timer_reset = speed;
        /* A byte-cursor read avoids a second saved pointer for the delay. */
        channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].delay_reset = note_data[2];
        channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].fade_step = 0x100;
        channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].step = suzuki_smd_modulator_step_triangle_centered;
        channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].waveform = SUZUKI_MODULATOR_WAVEFORM_TRIANGLE_CENTERED;
        channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].target = SUZUKI_MODULATOR_TARGET_PITCH;
        channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].flags
            = SUZUKI_MODULATOR_RESTART_ON_KEY_ON | SUZUKI_MODULATOR_ACTIVE;
        suzuki_smd_modulator_reset(&channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO]);
    }
    return note_data + sizeof(*parameters);
}
