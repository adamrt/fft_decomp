#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xE4: programs modulator 1 (volume) as
 * a triangle (waveform 2) with the given speed, signed depth and delay, and
 * restarts it. The depth is negated before scaling. */
u8* main_smd_tremolo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s32 depth;
    s16 speed;
    suzuki_modulator_t* modulator;

    depth = (s8)note_data[1];
    speed = note_data[0];
    if (depth != 0 && speed != 0) {
        modulator = &channel->modulators[1];
        modulator->amplitude = main_smd_modulator_calculate_step(-depth << 24, speed, 2);
        modulator->timer_reset = speed;
        modulator->counter_14_reset = note_data[2];
        modulator->counter_18_reset = 0x100;
        modulator->step = main_smd_modulator_step_triangle;
        modulator->waveform = 2;
        modulator->target = 1;
        modulator->flags = 3;
        main_smd_modulator_reset(modulator);
    }
    return note_data + 3;
}
