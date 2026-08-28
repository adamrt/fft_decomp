#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xD9: like Pitch Shift (0xD8), but the
 * third byte selects the waveform (low nibble) instead of the delay, which
 * is cleared; flags bit 1 is set when bit 4 of that byte is clear.
 *
 * Taking the modulator pointer before the step calculation call keeps it a
 * separate pseudo (combine does not merge across the call), which places
 * the reset argument where the target has it. */
u8* main_smd_pitch_shift_waveform(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    s32 depth;
    s16 speed;
    u16 waveform;
    s32 flags;
    suzuki_modulator_t* modulator;

    depth = (s8)note_data[1];
    waveform = note_data[2];
    speed = note_data[0];
    if (depth != 0 && speed != 0) {
        if (depth < 0) {
            depth = depth * -depth;
        } else {
            depth = depth * depth;
        }
        modulator = &channel->modulators[0];
        flags = ((waveform & 0x10) == 0) << 1;
        waveform &= 0xf;
        modulator->amplitude = main_smd_modulator_calculate_step(depth << 14, speed, waveform);
        modulator->timer_reset = speed;
        modulator->counter_14_reset = 0;
        modulator->counter_18_reset = 0x100;
        modulator->step = g_main_smd_modulator_waveforms[waveform];
        modulator->waveform = waveform;
        modulator->target = 0;
        modulator->flags = flags + 1;
        main_smd_modulator_reset(modulator);
    }
    return note_data + 3;
}
