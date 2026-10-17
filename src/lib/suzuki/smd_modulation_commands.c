/* SMD pitch, volume, balance and modulator commands, sound-font selection and track release. */

#include "fft/suzuki.h"
#include "psx/libspu.h"
#include "psx/types.h"

typedef s32 (*suzuki_modulator_step_t)(suzuki_modulator_t* modulator);

extern suzuki_modulator_step_t g_suzuki_smd_modulator_waveforms[16]; /* 0x80028f54 */

/* Vibrato, tremolo and pan-LFO operands with a start delay. */
typedef struct suzuki_smd_modulator_parameters {
    u8 speed;
    s8 depth;
    u8 delay;
} suzuki_smd_modulator_parameters_t;
typedef char suzuki_smd_modulator_parameters_size_must_be_3[sizeof(suzuki_smd_modulator_parameters_t) == 3 ? 1 : -1];

/* Vibrato, tremolo and pan-LFO waveform operands. */
typedef struct suzuki_smd_waveform_parameters {
    u8 speed;
    s8 depth;
    u8 waveform_flags; /* low four bits: waveform; bit 4: no restart on key-on */
} suzuki_smd_waveform_parameters_t;
typedef char suzuki_smd_waveform_parameters_size_must_be_3[(sizeof(suzuki_smd_waveform_parameters_t) == 3) ? 1 : -1];

/* Encoded waveform byte of the D9, E5, ED and F0 SMD opcodes. */
enum { SUZUKI_MODULATOR_INPUT_WAVEFORM_MASK = 0x0f, SUZUKI_MODULATOR_INPUT_NO_KEY_ON_RESTART = 0x10 };

/* SMD opcode 0xD0 "Pitch Offset": sets the channel pitch offset from a
 * signed byte in 1/8 semitone units. */
u8* suzuki_smd_pitch_offset(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->pitch_offset = (s8)note_data[0] << 5;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_PITCH;
    return note_data + 1;
}

/* SMD opcode 0xD1: add to the channel pitch offset in
 * 1/8 semitone units (32 of 256). Coarse twin of suzuki_smd_add_pitch_offset_fine. */
u8* suzuki_smd_add_pitch_offset_coarse(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->pitch_offset += (s8)note_data[0] << 5;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_PITCH;
    return note_data + 1;
}

/* SMD opcode 0xD2: add to the channel pitch offset in
 * 1/32 semitone units (8 of 256). Fine twin of suzuki_smd_add_pitch_offset_coarse. */
u8* suzuki_smd_add_pitch_offset_fine(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->pitch_offset += (s8)note_data[0] << 3;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_PITCH;
    return note_data + 1;
}

/* SMD opcode 0xD3 "Shift Pitch Offset (16-bit)": adds a signed 16-bit delta in
 * 1/256 semitone units to the channel pitch offset, carried as a signed high
 * byte followed by an unsigned low byte. */
u8* suzuki_smd_add_pitch_offset_wide(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s8 high = note_data[0];
    u8 low = note_data[1];

    channel->pitch_offset += low + (high << 8);
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_PITCH;
    return note_data + 2;
}

/* SMD opcode 0xD4 "Pitch Slide": slides the pitch by a signed number of
 * semitones over the given number of ticks; a zero count or amount stops the
 * slide. */
u8* suzuki_smd_pitch_slide(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 steps = note_data[0];
    s32 amount = (s8)note_data[1] << 24;

    if (steps != 0 && amount != 0) {
        channel->pitch_slide_count = steps;
        channel->ramp_flags |= SUZUKI_RAMP_PITCH_SLIDE;
        channel->pitch_slide_step = amount / steps;
    } else {
        channel->ramp_flags &= ~SUZUKI_RAMP_PITCH_SLIDE;
    }
    return note_data + 2;
}

/* SMD opcode 0xD5 "Pitch Slide Hold": toggles the hold that keeps a pitch
 * slide going past its count. */
u8* suzuki_smd_toggle_pitch_slide_hold(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->ramp_flags ^= SUZUKI_RAMP_PITCH_SLIDE_HOLD;
    return note_data;
}

/* SMD opcode 0xDC "Pitch Slide Off": stops a pitch slide where it is. */
u8* suzuki_smd_pitch_slide_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->ramp_flags &= ~SUZUKI_RAMP_PITCH_SLIDE;
    return note_data;
}

/* SMD opcode 0xD6 "Portamento": the ticks each key-on takes to slide from
 * the previous key (suzuki_smd_read_instructions); 0 turns it off. */
u8* suzuki_smd_portamento(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 value = *note_data++;

    channel->portamento_ticks = value;
    if (value != 0)
        channel->ramp_flags |= SUZUKI_RAMP_PORTAMENTO;
    else
        channel->ramp_flags &= ~SUZUKI_RAMP_PORTAMENTO;
    return note_data;
}

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

/* SMD opcode 0xD7 "Vibrato Fade In": modulator 0 fade_level and its reset
 * value become 0x100 / (byte + 1). The duration wraps at 256;
 * byte 0xff leaves both values unchanged. */
u8* suzuki_smd_pitch_shift_fade_in(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 duration = note_data[0] + 1;
    s16 step;

    note_data++;
    if (duration != 0) {
        step = 0x100 / duration;
        channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].fade_step = step;
        channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].fade_level = step;
    }
    return note_data;
}

/* SMD opcode 0xDA "Vibrato On": activates modulator 0. */
u8* suzuki_smd_pitch_shift_on(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].flags |= SUZUKI_MODULATOR_ACTIVE;
    return note_data;
}

/* SMD opcode 0xDB "Vibrato Off": deactivates modulator 0. */
u8* suzuki_smd_pitch_shift_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].flags &= ~SUZUKI_MODULATOR_ACTIVE;
    return note_data;
}

/* SMD opcode 0xE0 "Dynamic": absolute channel volume. It ends any volume
 * ramp. */
u8* suzuki_smd_dynamic(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->volume = note_data[0] << 24;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
    channel->ramp_flags &= ~SUZUKI_RAMP_VOLUME;
    return note_data + 1;
}

/* SMD opcode 0xE1 "Crescendo": signed volume delta. Like "Dynamic"
 * (0xE0) it marks a volume change and ends any volume ramp. */
u8* suzuki_smd_crescendo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s8 volume_delta = note_data[0];

    channel->volume = (channel->volume + (volume_delta << 24)) & 0x7fffffff;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
    channel->ramp_flags &= ~SUZUKI_RAMP_VOLUME;
    return note_data + 1;
}

/* SMD opcode 0xE2 "Volume Ramp": ramps the channel volume to a signed
 * target byte over the given number of ticks. It holds no note. */
u8* suzuki_smd_fermata_ramp(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 steps = note_data[0];
    s32 delta = ((s8)note_data[1] << 24) - channel->volume;

    if (steps != 0 && delta != 0) {
        channel->volume_step_count = steps;
        channel->ramp_flags |= SUZUKI_RAMP_VOLUME;
        channel->volume_step = delta / steps;
    }
    return note_data + 2;
}

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

/* SMD opcode 0xE5 "Tremolo Waveform": like Tremolo (0xE4), but the third
 * byte selects the waveform (low nibble) instead of the delay, which is
 * cleared; the modulator restarts on key-on unless bit 4 of that byte is set.
 *
 * Taking the modulator pointer before the step calculation call keeps it a
 * separate pseudo (combine does not merge across the call), which places
 * the reset argument where the target has it. */
u8* suzuki_smd_tremolo_waveform(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    const suzuki_smd_waveform_parameters_t* parameters = (const suzuki_smd_waveform_parameters_t*)note_data;
    s8 depth;
    s16 speed;
    u16 waveform = parameters->waveform_flags;
    s32 flags;
    suzuki_modulator_t* modulator;

    depth = parameters->depth;
    speed = parameters->speed;
    if (depth != 0 && speed != 0) {
        modulator = &channel->modulators[SUZUKI_MODULATOR_SLOT_TREMOLO];
        flags = (waveform & SUZUKI_MODULATOR_INPUT_NO_KEY_ON_RESTART) ? 0 : SUZUKI_MODULATOR_RESTART_ON_KEY_ON;
        waveform &= SUZUKI_MODULATOR_INPUT_WAVEFORM_MASK;
        modulator->amplitude = suzuki_smd_modulator_calculate_step(-depth << 24, speed, waveform);
        modulator->timer_reset = speed;
        modulator->delay_reset = 0;
        modulator->fade_step = 0x100;
        modulator->step = g_suzuki_smd_modulator_waveforms[waveform];
        modulator->waveform = waveform;
        modulator->target = SUZUKI_MODULATOR_TARGET_VOLUME;
        modulator->flags = flags + SUZUKI_MODULATOR_ACTIVE;
        suzuki_smd_modulator_reset(modulator);
    }
    return note_data + sizeof(*parameters);
}

/* SMD opcode 0xE3 "Tremolo Fade In": modulator 1 fade_level and its reset
 * value become 0x100 / (byte + 1), like 0xD7 does for modulator 0.
 * Byte 0xff leaves both values unchanged. */
u8* suzuki_smd_tremolo_fade_in(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 duration = note_data[0] + 1;
    s16 step;

    note_data++;
    if (duration != 0) {
        step = 0x100 / duration;
        channel->modulators[SUZUKI_MODULATOR_SLOT_TREMOLO].fade_step = step;
        channel->modulators[SUZUKI_MODULATOR_SLOT_TREMOLO].fade_level = step;
    }
    return note_data;
}

/* SMD opcode 0xE6 "Tremolo On": activates modulator 1. */
u8* suzuki_smd_tremolo_on(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[SUZUKI_MODULATOR_SLOT_TREMOLO].flags |= SUZUKI_MODULATOR_ACTIVE;
    return note_data;
}

/* SMD opcode 0xE7 "Tremolo Off": deactivates modulator 1. */
u8* suzuki_smd_tremolo_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[SUZUKI_MODULATOR_SLOT_TREMOLO].flags &= ~SUZUKI_MODULATOR_ACTIVE;
    return note_data;
}

/* SMD opcode 0xE8 "Balance": 0 is left, 0x40 the center, 0x7f right. */
u8* suzuki_smd_balance(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->balance = note_data[0] << 8;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
    return note_data + 1;
}

/* SMD opcode 0xE9 "Shift Balance": signed balance delta. The byte is
 * fetched unsigned and sign-extended afterwards, as in "Balance". */
u8* suzuki_smd_shift_balance(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s8 delta = note_data[0];

    channel->balance = (channel->balance + (delta << 8)) & 0x7fff;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
    return note_data + 1;
}

/* SMD opcode 0xEA "Balance Ramp": ramps the balance to a target byte over
 * count ticks. balance_step is the per-tick delta that
 * suzuki_smd_update_channel_ramps adds while SUZUKI_RAMP_BALANCE is set. */
u8* suzuki_smd_balance_ramp(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 count = note_data[0];
    s32 delta = (s8)note_data[1] - (channel->balance >> 8);

    if (count != 0 && delta != 0) {
        channel->balance_step_count = count;
        channel->ramp_flags |= SUZUKI_RAMP_BALANCE;
        channel->balance_step = (delta << 8) / count;
    }
    return note_data + 2;
}

/* SMD opcode 0xEB "Pan LFO Fade In": modulator 2 fade_level and its reset
 * value become 0x100 / (byte + 1). Twin of suzuki_smd_pitch_shift_fade_in;
 * byte 0xff leaves both values unchanged. */
u8* suzuki_smd_pan_lfo_fade_in(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 duration = note_data[0] + 1;
    s16 step;

    note_data++;
    if (duration != 0) {
        step = 0x100 / duration;
        channel->modulators[SUZUKI_MODULATOR_SLOT_PAN_LFO].fade_step = step;
        channel->modulators[SUZUKI_MODULATOR_SLOT_PAN_LFO].fade_level = step;
    }
    return note_data;
}

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

/* SMD opcode 0xEE "Pan LFO On": activates modulator 2. */
u8* suzuki_smd_pan_lfo_on(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[SUZUKI_MODULATOR_SLOT_PAN_LFO].flags |= SUZUKI_MODULATOR_ACTIVE;
    return note_data;
}

/* SMD opcode 0xEF "Pan LFO Off": deactivates modulator 2. */
u8* suzuki_smd_pan_lfo_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[SUZUKI_MODULATOR_SLOT_PAN_LFO].flags &= ~SUZUKI_MODULATOR_ACTIVE;
    return note_data;
}

/* SMD opcode 0xF0 "Modulator Select": selects the modulator that 0xF1 and
 * 0xF2 program and sets its waveform (low nibble of the second byte; it
 * restarts on key-on unless bit 4 is set) and output target (third byte,
 * suzuki_modulator_target_e). The modulator is left inactive. */
u8* suzuki_smd_modulator_select(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator;
    u8 param;

    channel->modulator_index = note_data[0];
    modulator = &channel->modulators[channel->modulator_index];
    param = note_data[1];
    modulator->waveform = param & SUZUKI_MODULATOR_INPUT_WAVEFORM_MASK;
    modulator->step = g_suzuki_smd_modulator_waveforms[modulator->waveform];
    if ((param & SUZUKI_MODULATOR_INPUT_NO_KEY_ON_RESTART) == 0) {
        modulator->flags = SUZUKI_MODULATOR_RESTART_ON_KEY_ON;
    } else {
        modulator->flags = 0;
    }
    modulator->target = note_data[2];
    modulator->delay_reset = 0;
    modulator->fade_step = 0x100;
    return note_data + 3;
}

/* SMD opcode 0xF1 "Modulator Set Depth": sets the speed and the depth of
 * the modulator selected by 0xF0. Parameters: speed, then a signed 16-bit
 * depth (high byte first) scaled to the high 16 bits of the 32-bit value. */
u8* suzuki_smd_modulator_set_depth(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator = &channel->modulators[channel->modulator_index];
    s16 speed = note_data[0];

    modulator->amplitude = suzuki_smd_modulator_calculate_step(
        ((s8)note_data[1] << 24) | (note_data[2] << 16), speed, modulator->waveform);
    modulator->timer_reset = speed;
    return note_data + 3;
}

/* Returns the amplitude a modulator steps by so that its waveform spans
 * depth over one timer period of speed steps: depth / speed for the triangle
 * waveforms (2-3), depth / (speed - 1) for the sawtooth (4, which resets on
 * the last step) and depth itself for the other waveforms or when depth or
 * speed is zero. */
s32 suzuki_smd_modulator_calculate_step(s32 depth, s16 speed, s16 waveform) {
    if (depth != 0 && speed != 0) {
        switch (waveform) {
        case SUZUKI_MODULATOR_WAVEFORM_TRIANGLE:
        case SUZUKI_MODULATOR_WAVEFORM_TRIANGLE_CENTERED:
            depth /= speed;
            break;
        case SUZUKI_MODULATOR_WAVEFORM_SAWTOOTH:
            if (speed != 1) {
                depth /= speed - 1;
            }
            break;
        }
    }
    return depth;
}

/* SMD opcode 0xF2 "Modulator Set Fade In": for the modulator selected by
 * 0xF0, the first byte is the start delay and fade_level and its reset value
 * become 0x100 / (second byte + 1). Generic twin of 0xD7, 0xE3 and 0xEB;
 * second byte 0xff leaves the delay and both fade values unchanged. */
u8* suzuki_smd_modulator_set_fade_in(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator;
    u8 duration = note_data[1] + 1;
    s16 step;

    modulator = &channel->modulators[channel->modulator_index];
    if (duration != 0) {
        step = 0x100 / duration;
        modulator->delay_reset = note_data[0];
        modulator->fade_step = step;
        modulator->fade_level = step;
    }
    return note_data + 2;
}

/* SMD opcode 0xF5 "Modulator Enable Mask": bit n of the parameter restarts
 * and activates modulator n, a clear bit deactivates it (modulators 0-3). */
u8* suzuki_smd_modulator_enable_mask(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator = channel->modulators;
    s32 i = SUZUKI_MODULATOR_COUNT;
    u8 mask = *note_data++;

    do {
        if (mask & 1) {
            suzuki_smd_modulator_reset(modulator);
            modulator->flags |= SUZUKI_MODULATOR_ACTIVE;
        } else {
            modulator->flags &= ~SUZUKI_MODULATOR_ACTIVE;
        }
        mask >>= 1;
        modulator++;
    } while (--i);
    return note_data;
}

/* SMD opcode 0xF6 "Modulator Enable": restarts and activates the modulator
 * selected by the parameter byte. */
u8* suzuki_smd_modulator_enable(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator = &channel->modulators[*note_data++];

    suzuki_smd_modulator_reset(modulator);
    modulator->flags |= SUZUKI_MODULATOR_ACTIVE;
    return note_data;
}

/* Restarts a modulator: called by the SMD opcodes that program one (the
 * vibrato, tremolo and pan LFO opcodes) or enable one (0xF5, 0xF6). */
void suzuki_smd_modulator_reset(suzuki_modulator_t* modulator) {
    modulator->timer = 1;
    modulator->value = 0;
    modulator->flags &= ~(SUZUKI_MODULATOR_NEGATED | SUZUKI_MODULATOR_HALF_PERIOD_DONE);
    modulator->delay = modulator->delay_reset;
    modulator->fade_level = modulator->fade_step;
}

/* SMD opcode 0xF7 "Modulator Disable": deactivates the modulator selected
 * by the parameter byte. */
u8* suzuki_smd_modulator_disable(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->modulators[*note_data].flags &= ~SUZUKI_MODULATOR_ACTIVE;
    return note_data + 1;
}

/* SMD opcode 0xFD "Scale Tempo": multiplies the tempo by its byte. A zero
 * byte leaves the tempo alone. */
u8* suzuki_smd_scale_tempo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 scale = *note_data++;

    if (scale != 0) {
        music->tempo_scale.value = scale << 24;
        music->scaled_tempo = (music->tempo >> 16) * (scale << 8);
    }
    return note_data;
}

/* SMD opcode 0xFE "Select Sound Font": stores the waveset id in the
 * MUS record and gives the channel the loaded waveset with that id, or the
 * first loaded waveset when none matches. id is s16 like the parameter of
 * suzuki_sound_find_waveset, whose search loop this repeats. */
u8* suzuki_smd_select_sound_font(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_waveset_t* waveset = g_suzuki_sound_waveset_list;
    s16 id = *note_data++;

    music->waveset_id = id;
    while (waveset != 0 && waveset->id != id) {
        waveset = waveset->next;
    }
    if (waveset == 0) {
        waveset = g_suzuki_sound_waveset_list;
    }
    channel->waveset = waveset;
    return note_data;
}

/* SMD opcode 0xFF "End Track": when the envelope of the channel's SPU voice
 * has reached zero, cancels its pending key-on and key-off, releases the
 * channel (flags = 0) and leaves the read position on the opcode. For the
 * SFX record the voice is queued for SFX key-off, dropped from the
 * LFO/noise/reverb voice masks and from g_suzuki_sound_sfx_reserved_voices, and its
 * LFO, noise and reverb marks are set so those voice masks are recalculated;
 * otherwise it is queued for music key-off. While the envelope is non-zero
 * the handler only returns the position after the opcode. */
u8* suzuki_smd_end_track(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s32 key_stat;
    s16 envx;
    u32 mask;

    SpuGetVoiceEnvelopeAttr(channel->voice, &key_stat, &envx);
    if (envx == 0) {
        channel->voice_update_flags &= ~(SUZUKI_VOICE_UPDATE_KEY_ON | SUZUKI_VOICE_UPDATE_KEY_OFF);
        if (music->status & SUZUKI_MUSIC_STATUS_SFX) {
            g_suzuki_sound_sfx_key_off_voices |= channel->voice_attr.voice;
            mask = ~channel->voice_attr.voice;
            music->lfo_voice_mask &= mask;
            music->noise_voice_mask &= mask;
            music->reverb_voice_mask &= mask;
            g_suzuki_sound_sfx_reserved_voices &= mask;
            channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN | SUZUKI_VOICE_ATTR_ATTACK | SUZUKI_VOICE_ATTR_PITCH;
        } else {
            g_suzuki_sound_music_key_off_voices |= channel->voice_attr.voice;
        }
        channel->flags = 0;
        note_data--;
    }
    return note_data;
}
