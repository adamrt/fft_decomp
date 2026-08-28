#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xF1: sets the speed and the depth of the
 * modulator selected by 0xF0. Parameters: speed, then a signed 16-bit depth
 * (high byte first) scaled to the top of the word. */
u8* main_smd_modulator_set_depth(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator;
    s16 speed;

    modulator = &channel->modulators[channel->modulator_index];
    speed = note_data[0];
    modulator->amplitude = main_smd_modulator_calculate_step(
        ((s8)note_data[1] << 24) | (note_data[2] << 16), speed, modulator->waveform);
    modulator->timer_reset = speed;
    return note_data + 3;
}
