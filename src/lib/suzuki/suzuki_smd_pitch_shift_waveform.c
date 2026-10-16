#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xD9 "Vibrato Waveform": like Vibrato (0xD8), but the third
 * byte selects the waveform (low nibble) instead of the delay, which is
 * cleared; the modulator restarts on key-on unless bit 4 of that byte is set.
 *
 * Taking the modulator pointer before the step calculation call keeps it a
 * separate pseudo (combine does not merge across the call), which places
 * the reset argument where the target has it. */
u8* suzuki_smd_pitch_shift_waveform(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    const suzuki_smd_waveform_parameters_t* parameters = (const suzuki_smd_waveform_parameters_t*)note_data;
    s32 depth;
    s16 speed;
    u16 waveform;
    s32 flags;
    suzuki_modulator_t* modulator;

    depth = parameters->depth;
    waveform = parameters->waveform_flags;
    speed = parameters->speed;
    if (depth != 0 && speed != 0) {
        if (depth < 0) {
            depth = depth * -depth;
        } else {
            depth = depth * depth;
        }
        modulator = &channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO];
        flags = (waveform & SUZUKI_MODULATOR_INPUT_NO_KEY_ON_RESTART) ? 0 : SUZUKI_MODULATOR_RESTART_ON_KEY_ON;
        waveform &= SUZUKI_MODULATOR_INPUT_WAVEFORM_MASK;
        modulator->amplitude = suzuki_smd_modulator_calculate_step(depth << 14, speed, waveform);
        modulator->timer_reset = speed;
        modulator->delay_reset = 0;
        modulator->fade_step = 0x100;
        modulator->step = g_suzuki_smd_modulator_waveforms[waveform];
        modulator->waveform = waveform;
        modulator->target = SUZUKI_MODULATOR_TARGET_PITCH;
        modulator->flags = flags + SUZUKI_MODULATOR_ACTIVE;
        suzuki_smd_modulator_reset(modulator);
    }
    return note_data + sizeof(*parameters);
}
