#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xED: as 0xEC, but the low nibble of the
 * third byte selects the waveform and the modulator starts without a delay;
 * flags bit 1 is set unless bit 4 of that byte is set.
 *
 * Matching notes: depth and param are reassigned in place (the loads lose
 * sched1's birthing boost and keep the target's load order, and param's
 * masking lands in $a2 before the copy back), and the reset argument is
 * taken before the calculate_step call, which places its `addiu` after the
 * 0x100 constant like the target. */
u8* main_smd_pan_lfo_waveform(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s32 depth;
    s16 speed;
    u16 param;
    s32 flags;
    suzuki_modulator_t* modulator;

    depth = (s8)note_data[1];
    param = note_data[2];
    speed = note_data[0];
    if (depth != 0 && speed != 0) {
        modulator = &channel->modulators[2];
        flags = ((param & 0x10) == 0) << 1;
        param &= 0xf;
        depth <<= 24;
        modulator->amplitude = main_smd_modulator_calculate_step(depth, speed, param);
        modulator->timer_reset = speed;
        modulator->counter_14_reset = 0;
        modulator->counter_18_reset = 0x100;
        modulator->step = g_main_smd_modulator_waveforms[param];
        modulator->waveform = param;
        modulator->target = 2;
        modulator->flags = flags + 1;
        main_smd_modulator_reset(modulator);
    }
    return note_data + 3;
}
