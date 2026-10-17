#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xED "Pan LFO Waveform": as Pan LFO (0xEC), but the low nibble
 * of the third byte selects the waveform and the modulator starts without a
 * delay; it restarts on key-on unless bit 4 of that byte is set.
 *
 * Matching notes: depth and waveform are reassigned in place (the loads lose
 * sched1's birthing boost and keep the target's load order, and waveform's
 * masking lands in $a2 before the copy back), and the reset argument is
 * taken before the calculate_step call, which places its `addiu` after the
 * 0x100 constant like the target. */
u8* suzuki_smd_pan_lfo_waveform(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
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
        modulator = &channel->modulators[SUZUKI_MODULATOR_SLOT_PAN_LFO];
        flags = (waveform & SUZUKI_MODULATOR_INPUT_NO_KEY_ON_RESTART) ? 0 : SUZUKI_MODULATOR_RESTART_ON_KEY_ON;
        waveform &= SUZUKI_MODULATOR_INPUT_WAVEFORM_MASK;
        depth <<= 24;
        modulator->amplitude = suzuki_smd_modulator_calculate_step(depth, speed, waveform);
        modulator->timer_reset = speed;
        modulator->delay_reset = 0;
        modulator->fade_step = 0x100;
        modulator->step = g_suzuki_smd_modulator_waveforms[waveform];
        modulator->waveform = waveform;
        modulator->target = SUZUKI_MODULATOR_TARGET_BALANCE;
        modulator->flags = flags + SUZUKI_MODULATOR_ACTIVE;
        suzuki_smd_modulator_reset(modulator);
    }
    return note_data + sizeof(*parameters);
}
