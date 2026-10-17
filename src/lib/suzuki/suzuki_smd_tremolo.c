#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xE4 "Tremolo": programs modulator 1 (volume) as a triangle
 * (waveform 2) with the given speed, signed depth and delay, and restarts
 * it. The depth is negated before scaling. */
u8* suzuki_smd_tremolo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    const suzuki_smd_modulator_parameters_t* parameters = (const suzuki_smd_modulator_parameters_t*)note_data;
    s8 depth;
    s16 speed;
    suzuki_modulator_t* modulator;

    depth = parameters->depth;
    speed = parameters->speed;
    if (depth != 0 && speed != 0) {
        modulator = &channel->modulators[SUZUKI_MODULATOR_SLOT_TREMOLO];
        modulator->amplitude
            = suzuki_smd_modulator_calculate_step(-depth << 24, speed, SUZUKI_MODULATOR_WAVEFORM_TRIANGLE);
        modulator->timer_reset = speed;
        modulator->delay_reset = parameters->delay;
        modulator->fade_step = 0x100;
        modulator->step = suzuki_smd_modulator_step_triangle;
        modulator->waveform = SUZUKI_MODULATOR_WAVEFORM_TRIANGLE;
        modulator->target = SUZUKI_MODULATOR_TARGET_VOLUME;
        modulator->flags = SUZUKI_MODULATOR_RESTART_ON_KEY_ON | SUZUKI_MODULATOR_ACTIVE;
        suzuki_smd_modulator_reset(modulator);
    }
    return note_data + sizeof(*parameters);
}
