/* SMD commands for tempo, channel articulation, SPU modes and envelopes. */

#include "fft/suzuki.h"
#include "psx/libspu.h"
#include "psx/types.h"

/* SMD opcode 0xA0 "Tempo". The root counter ticks 240 times a second and a
 * sequencer tick takes 0x10000 / (tempo * tempo scale) of them, 48 sequencer
 * ticks to a quarter note: at the default scale a tempo byte is worth 75/64
 * quarter notes a minute (the default 0x66 is 119.5). Handlers receive the
 * note-data pointer, the MUS record and the channel; this one only touches
 * the MUS record.
 * `volatile` keeps the redundant `andi 0xff` after the `lbu`. */
u8* suzuki_smd_tempo(u8* data, suzuki_music_t* music, suzuki_music_channel_t* unused_channel) {
    volatile u8* note_data = data;
    u8 tempo;

    tempo = note_data[0] & 0xff;
    music->tempo = tempo << 16;
    music->scaled_tempo = tempo * (music->tempo_scale.value >> 16);
    return data + 1;
}

/* SMD opcode 0xA1 "Accelerando": adds a signed tempo delta but clears scaled_tempo.
 * Zero is not a refresh request: only later tempo or tempo-scale updates recompute it. */
u8* suzuki_smd_accelerando(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* unused_channel) {
    s32 tempo_delta;
    char unused[8]; /* the target keeps an 8-byte frame */

    (void)unused;
    tempo_delta = *(s8*)note_data;
    music->scaled_tempo = 0;
    music->tempo += tempo_delta << 16;
    return note_data + 1;
}

/* SMD opcode 0xA2 "Tempo Ramp": moves the tempo to the target byte over the
 * given number of sequencer ticks (suzuki_smd_update_channel_ramps steps it).
 * A zero count or an unchanged tempo starts nothing. */
u8* suzuki_smd_tempo_ramp(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s16 steps;
    u32 target = note_data[1];
    u32 scaled;
    s32 delta;

    steps = note_data[0];
    music->tempo_target = target;
    scaled = target << 16;
    delta = scaled - music->tempo;
    if (steps != 0 && delta != 0) {
        music->tempo_step_count = steps;
        music->tempo_step = delta / steps;
    }
    return note_data + 2;
}

/* SMD opcode 0xA6 "Master Volume": sets the MUS master volume and marks
 * every channel for a volume update. */
u8* suzuki_smd_master_volume(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->master_volume.value = *note_data++ << 24;
    suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_VOLUME, music);
    return note_data;
}

/* SMD opcode 0xA7 "Master Volume Ramp": ramps the MUS master volume to a
 * target byte over 32 root-counter ticks per unit of the count byte. */
u8* suzuki_smd_master_volume_ramp(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s16 steps = note_data[0] << 5;
    u8 target = note_data[1];
    s32 delta = (target << 24) - music->master_volume.value;

    if (steps != 0 && delta != 0) {
        music->master_volume.count = steps;
        music->master_volume.target = target << 8;
        music->master_volume.step = delta / steps;
    }
    return note_data + 2;
}

/* SMD opcode 0xA9 "Gate Time": how many sixteenths of its length each note
 * stays keyed on (suzuki_smd_read_instructions; 0x10 is the whole length). */
u8* suzuki_smd_gate_time(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->gate_time = *note_data;
    return note_data + 1;
}

/* SMD opcode 0xAA "Set Voice": assigns the channel a fixed SPU voice. A
 * voice number below 25 sets voice_mask to 1 << voice and marks every voice
 * attribute for update; a larger number clears both. */
u8* suzuki_smd_set_voice(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 voice = *note_data++;

    channel->voice = voice;
    if (voice < 25) {
        channel->voice_attr.voice = SPU_VOICECH(channel->voice);
        channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ALL;
    } else {
        channel->voice_attr.voice = 0;
        channel->voice_attr_flags = 0;
    }
    return note_data;
}

/* SMD opcode 0xAC "Instrument". */
u8* suzuki_smd_instrument(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_smd_set_instrument(*note_data++, channel);
    return note_data;
}

/* SMD opcode 0xAD "Length Adjust": a non-zero byte is added to
 * length_adjust (signed, as the member is s8), a zero byte resets it. */
u8* suzuki_smd_length_adjust(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 delta = *note_data++;

    if (delta != 0)
        channel->length_adjust += delta;
    else
        channel->length_adjust = 0;
    return note_data;
}

/* SMD opcode 0xAE "Nop": consumes no parameter. */
u8* suzuki_smd_opcode_ae(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    return note_data;
}

/* SMD opcode 0xAF "Nop": consumes no parameter. */
u8* suzuki_smd_opcode_af(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    return note_data;
}

/* SMD opcode 0xB0 "Slur On": every following note is tied to the next. */
u8* suzuki_smd_slur_on(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->flags |= SUZUKI_CHANNEL_SLUR;
    return note_data;
}

/* SMD opcode 0xB1 "Slur Off". */
u8* suzuki_smd_slur_off(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->flags &= ~SUZUKI_CHANNEL_SLUR;
    return note_data;
}

/* SMD opcode 0xB2 "Enable LFO": adds the channel's voice to the SPU
 * pitch-LFO voices, if its voice number is odd. */
u8* suzuki_smd_enable_lfo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    if ((channel->voice & 1) != 0) {
        music->lfo_voice_mask |= channel->voice_attr.voice;
        channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_PITCH;
    }
    return note_data;
}

/* SMD opcode 0xB3 "Disable LFO". */
u8* suzuki_smd_disable_lfo(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->lfo_voice_mask &= ~channel->voice_attr.voice;
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_PITCH;
    return note_data;
}

/* SMD opcode 0xB4 "Enable Noise": sets the MUS noise clock,
 * programs it with SpuSetNoiseClock and adds the channel's voice to the
 * noise voices. */
u8* suzuki_smd_enable_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->noise_clock = *note_data++;
    SpuSetNoiseClock(music->noise_clock);
    music->noise_voice_mask |= channel->voice_attr.voice;
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ATTACK;
    return note_data;
}

/* SMD opcode 0xB5 "Increase Noise": adds its byte to the MUS
 * noise clock (modulo 64), then acts like Enable Noise (0xB4). */
u8* suzuki_smd_increase_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->noise_clock = (*note_data++ + music->noise_clock) & 0x3f;
    SpuSetNoiseClock(music->noise_clock);
    music->noise_voice_mask |= channel->voice_attr.voice;
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ATTACK;
    return note_data;
}

/* SMD opcode 0xB6 "Turn On Noise". */
u8* suzuki_smd_turn_on_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->noise_voice_mask |= channel->voice_attr.voice;
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ATTACK;
    return note_data;
}

/* SMD opcode 0xB7 "Disable Noise". */
u8* suzuki_smd_disable_noise(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->noise_voice_mask &= ~channel->voice_attr.voice;
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ATTACK;
    return note_data;
}

/* SMD opcode 0xB8 "Acoustics": reverb depth, delay and feedback, passed on
 * with reverb mode 10, for which suzuki_sound_set_reverb_mode only records
 * them. */
u8* suzuki_smd_acoustics(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    s16 depth = note_data[0] << 8;
    s8 delay;
    s8 feedback;

    music->reverb_depth = depth;
    delay = note_data[1];
    music->reverb_delay = delay;
    feedback = note_data[2];
    music->reverb_feedback = feedback;
    suzuki_sound_set_reverb_mode(10, depth, delay, feedback);
    return note_data + 3;
}

/* SMD opcode 0xBA "Enable Reverb". */
u8* suzuki_smd_enable_reverb(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->reverb_voice_mask |= channel->voice_attr.voice;
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN;
    return note_data;
}

/* SMD opcode 0xBB "Disable Reverb". */
u8* suzuki_smd_disable_reverb(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->reverb_voice_mask &= ~channel->voice_attr.voice;
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN;
    return note_data;
}

/* SMD opcode 0xC0 "Naturale": reloads the channel's current instrument,
 * undoing the envelope opcodes since it was set. */
u8* suzuki_smd_naturale(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_smd_set_instrument(channel->instrument, channel);
    return note_data;
}

/* SMD opcode 0xC1 "Envelope Shape": attack, sustain and release
 * modes. */
u8* suzuki_smd_envelope_shape(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 release_mode;

    channel->voice_attr.a_mode = note_data[0];
    channel->voice_attr.s_mode = note_data[1];
    release_mode = note_data[2];
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN_LEVEL | SUZUKI_VOICE_ATTR_RELEASE | SUZUKI_VOICE_ATTR_SUSTAIN
        | SUZUKI_VOICE_ATTR_DECAY | SUZUKI_VOICE_ATTR_ATTACK;
    channel->voice_attr.r_mode = release_mode;
    return note_data + 3;
}

/* SMD opcode 0xC2 "Attack Rate". */
u8* suzuki_smd_attack_time(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 value = *note_data++;

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ATTACK;
    channel->voice_attr.ar = value;
    return note_data;
}

/* SMD opcode 0xC3 "Decay Rate". */
u8* suzuki_smd_decay_time(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 value = *note_data++;

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_DECAY;
    channel->voice_attr.dr = value;
    return note_data;
}

/* SMD opcode 0xC4 "Sustain Rate". */
u8* suzuki_smd_sustain_time(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 value = *note_data++;

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN;
    channel->voice_attr.sr = value;
    return note_data;
}

/* SMD opcode 0xC5 "Release Rate". The byte lands both in
 * note_release_rate, which every note reloads, and in release_rate. */
u8* suzuki_smd_release(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 release = note_data[0];

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_RELEASE;
    channel->note_release_rate = release;
    channel->voice_attr.rr = release;
    return note_data + 1;
}

/* SMD opcode 0xC6 "Sustain Level". */
u8* suzuki_smd_sustain_level(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 level = note_data[0];

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN_LEVEL;
    channel->voice_attr.sl = level;
    return note_data + 1;
}

/* SMD opcode 0xC7 "Envelope Tail": decay rate then sustain level. */
u8* suzuki_smd_envelope_tail(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 level;

    channel->voice_attr.dr = note_data[0];
    level = note_data[1];
    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN_LEVEL | SUZUKI_VOICE_ATTR_DECAY;
    channel->voice_attr.sl = level;
    return note_data + 2;
}

/* SMD opcode 0xC8 "Attack Mode". */
u8* suzuki_smd_attack_mode(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 mode = note_data[0];

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ATTACK;
    channel->voice_attr.a_mode = mode;
    return note_data + 1;
}

/* SMD opcode 0xC9 "Sustain Mode". */
u8* suzuki_smd_sustain_mode(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 mode = note_data[0];

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_SUSTAIN;
    channel->voice_attr.s_mode = mode;
    return note_data + 1;
}

/* SMD opcode 0xCA "Release Mode". */
u8* suzuki_smd_release_mode(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 mode = note_data[0];

    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_RELEASE;
    channel->voice_attr.r_mode = mode;
    return note_data + 1;
}
