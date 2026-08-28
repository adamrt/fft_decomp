#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xEC: programs and restarts modulator 2
 * (balance) with the centred triangle waveform. Parameters: speed, signed
 * depth, start delay. A zero speed or depth leaves the modulator unchanged.
 *
 * speed is s16 like the calculate_step parameter: the target tests and
 * passes a copy of it and keeps the original for timer_reset. */
u8* main_smd_pan_lfo(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    s32 depth;
    s16 speed;
    suzuki_modulator_t* modulator;

    depth = (s8)note_data[1];
    speed = note_data[0];
    if (depth != 0 && speed != 0) {
        modulator = &channel->modulators[2];
        modulator->amplitude = main_smd_modulator_calculate_step(depth << 24, speed, 3);
        modulator->timer_reset = speed;
        modulator->counter_14_reset = note_data[2];
        modulator->counter_18_reset = 0x100;
        modulator->step = main_smd_modulator_step_triangle_centered;
        modulator->waveform = 3;
        modulator->target = 2;
        modulator->flags = 3;
        main_smd_modulator_reset(modulator);
    }
    return note_data + 3;
}
