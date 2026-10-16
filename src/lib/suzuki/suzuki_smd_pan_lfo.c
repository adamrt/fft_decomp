#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xEC "Pan LFO": programs and restarts modulator 2 (balance)
 * with the centered triangle waveform. Parameters: speed, signed depth, start
 * delay. A zero speed or depth leaves the modulator unchanged.
 *
 * speed is s16 like the calculate_step parameter: the target tests and
 * passes a copy of it and keeps the original for timer_reset. */
u8* suzuki_smd_pan_lfo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    const suzuki_smd_modulator_parameters_t* parameters = (const suzuki_smd_modulator_parameters_t*)note_data;
    s8 depth;
    s16 speed;
    suzuki_modulator_t* modulator;

    depth = parameters->depth;
    speed = parameters->speed;
    if (depth != 0 && speed != 0) {
        modulator = &channel->modulators[SUZUKI_MODULATOR_SLOT_PAN_LFO];
        modulator->amplitude
            = suzuki_smd_modulator_calculate_step(depth << 24, speed, SUZUKI_MODULATOR_WAVEFORM_TRIANGLE_CENTERED);
        modulator->timer_reset = speed;
        modulator->delay_reset = parameters->delay;
        modulator->fade_step = 0x100;
        modulator->step = suzuki_smd_modulator_step_triangle_centered;
        modulator->waveform = SUZUKI_MODULATOR_WAVEFORM_TRIANGLE_CENTERED;
        modulator->target = SUZUKI_MODULATOR_TARGET_BALANCE;
        modulator->flags = SUZUKI_MODULATOR_RESTART_ON_KEY_ON | SUZUKI_MODULATOR_ACTIVE;
        suzuki_smd_modulator_reset(modulator);
    }
    return note_data + sizeof(*parameters);
}
