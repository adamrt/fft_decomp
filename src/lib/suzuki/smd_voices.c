/* Apply sequencer channels to SPU voices: instrument attributes, stereo volume,
 * note pitch and key-on/off masks. The waveform routines drive vibrato, tremolo
 * and pan modulation for both music and sound effects. */

#include "fft/suzuki.h"
#include "psx/types.h"

/* Ordinary driver-status view; the transfer wait keeps its measured volatile view. */
extern s16 g_suzuki_sound_driver_flags;

/* Key / 12 and key % 12 for keys 0 through 116. Both original 120-byte table
 * views end in three zero bytes; they do not continue either formula. */
#define KEY_OCTAVE(n) n, n, n, n, n, n, n, n, n, n, n, n
static u8 s_suzuki_smd_key_octaves[120] = { KEY_OCTAVE(0), KEY_OCTAVE(1), KEY_OCTAVE(2), KEY_OCTAVE(3), KEY_OCTAVE(4),
    KEY_OCTAVE(5), KEY_OCTAVE(6), KEY_OCTAVE(7), KEY_OCTAVE(8), 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 0 };
#undef KEY_OCTAVE

#define KEY_SEMITONES 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
static u8 s_suzuki_smd_key_semitones[120] = { KEY_SEMITONES, KEY_SEMITONES, KEY_SEMITONES, KEY_SEMITONES, KEY_SEMITONES,
    KEY_SEMITONES, KEY_SEMITONES, KEY_SEMITONES, KEY_SEMITONES, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0 };
#undef KEY_SEMITONES

/* Octave-six SPU pitch at 1/256-semitone intervals:
 * round(8192 * 2^(i / 3072)). The runtime only indexes this table. */
static u16 s_suzuki_smd_pitch_table[12 * 256] = {
#include "suzuki_pitch.inc"
};

/* Loads instrument entry n of the channel's waveset into the channel:
 * sample and loop addresses, ADSR rates and modes, sustain level and
 * fine_tune. An SFX or immediate-instrument channel gets every voice
 * attribute marked for update at once; otherwise the next note marks them
 * (SUZUKI_CHANNEL_INSTRUMENT_PENDING). The entry index is the instrument
 * number sign-extended from 16 bits, and the loop address is start +
 * loop_offset without the waveset's SPU base that start_address gets.
 *
 * Most music channels on the disc select instrument 0xff before their End
 * Bar. WAVESET.WD has 176 instruments and only its 0xb30-byte header is
 * kept on the heap, so that entry is read 0x4f0 bytes past the copy. */
void suzuki_smd_set_instrument(s16 instrument, suzuki_music_channel_t* channel) {
    suzuki_instrument_t* entry;
    suzuki_waveset_t* waveset;

    channel->instrument = instrument;
    waveset = channel->waveset;
    entry = &waveset->instruments[instrument];
    channel->voice_attr.addr = entry->start + waveset->spu_address;
    channel->voice_attr.loop_addr = entry->loop_offset + entry->start;
    channel->voice_attr.a_mode = entry->attack_mode;
    channel->voice_attr.s_mode = entry->sustain_mode;
    channel->voice_attr.r_mode = entry->release_mode;
    channel->voice_attr.ar = entry->attack_rate;
    channel->voice_attr.dr = entry->decay_rate;
    channel->voice_attr.sr = entry->sustain_rate;
    channel->voice_attr.rr = channel->note_release_rate = entry->release_rate;
    channel->voice_attr.sl = entry->sustain_level;
    channel->fine_tune = entry->fine_tune;
    if (channel->flags & (SUZUKI_CHANNEL_SFX | SUZUKI_CHANNEL_IMMEDIATE_INSTRUMENT)) {
        channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME | SUZUKI_VOICE_UPDATE_PITCH;
        channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ALL;
    } else {
        channel->flags |= SUZUKI_CHANNEL_INSTRUMENT_PENDING;
    }
}

/* ORs mask into voice_update_flags of every channel that is in use. */
void suzuki_smd_set_voice_update_flags_all_channels(u16 mask, suzuki_music_t* music) {
    suzuki_music_channel_t* channel = music->channels;
    s32 count = music->channel_count;

    do {
        if (channel->flags != 0)
            channel->voice_update_flags |= mask;
        channel++;
    } while (--count != 0);
}

/* ORs mask into voice_attr_flags of every channel that is in use (callers
 * pass 0x7000, which nothing tests, with or without SUZUKI_VOICE_ATTR_ALL). */
void suzuki_smd_set_voice_attr_flags_all_channels(suzuki_music_t* music, u16 mask) {
    suzuki_music_channel_t* channel = music->channels;
    u16 voice_attr_flags;
    s32 count = music->channel_count;

    do {
        if (channel->flags != 0) {
            voice_attr_flags = channel->voice_attr_flags; /* temp keeps `or rd,flags,mask` operand order */
            channel->voice_attr_flags = voice_attr_flags | mask;
        }
        channel++;
    } while (--count != 0);
}

/* Per-tick voice pass run after the SMD interpreter and the modulators.
 * SUZUKI_VOICE_UPDATE_VOLUME rebuilds the SPU voice volume: channel volume
 * plus its modulation (clamped to 0-0x7fff), scaled by velocity and the MUS
 * master volume, then split by balance (channel, modulation and MUS balance
 * shift, clamped to 0-0x7f00) into left/right levels when the driver runs in
 * stereo, or both set to 0x5a00/0x8000 of the volume in mono.
 * SUZUKI_VOICE_UPDATE_PITCH converts the pitch (with its modulation and the
 * MUS pitch shift) to an SPU pitch. The VOLUME and PITCH voice attribute
 * marks hand the results to suzuki_sound_update_voice_attrs. KEY_ON and
 * KEY_OFF queue the channel's voices for key on (unless muted) and key off
 * in the MUS masks, filtered by the voices SFX own unless this is the SFX
 * record.
 *
 * The channel pointer is a copy of the parameter so the count parameter is
 * copied first in the prologue, as in the target. */
void suzuki_smd_update_voices(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count) {
    suzuki_music_channel_t* channel = channels;
    u32 key_on;
    u32 key_off;
    u32 allowed;
    u16 voice_update_flags;
    s32 volume;
    s32 balance;
    s32 left;
    s32 right;

    if ((music->status & SUZUKI_MUSIC_STATUS_SFX) == 0)
        allowed = ~(g_suzuki_sound_sfx_reserved_voices | g_suzuki_sound_sfx_key_off_voices);
    else
        allowed = -1;
    key_off = 0;
    key_on = 0;
    do {
        if (channel->flags != 0 && channel->voice < 25) {
            voice_update_flags = channel->voice_update_flags;
            if ((voice_update_flags & SUZUKI_VOICE_UPDATE_VOLUME) != 0) {
                volume = (channel->volume >> 16) + channel->volume_modulation;
                if (volume > 0x7fff)
                    volume = 0x7fff;
                if (volume < 0)
                    volume = 0;
                volume = (channel->velocity * volume) >> 15;
                volume = ((music->master_volume.value >> 16) * volume) >> 16;
                balance = channel->balance + channel->balance_modulation + (music->balance_shift.value >> 16);
                if (balance > 0x7f00)
                    balance = 0x7f00;
                if (balance < 0)
                    balance = 0;
                if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_STEREO) != 0) {
                    if (balance < 0x4000) {
                        right = (balance * 0x5a00) >> 14;
                        left = 0x7f00 - ((balance * 0x2500) >> 14);
                    } else {
                        balance = 0x8000 - balance;
                        left = (balance * 0x5a00) >> 14;
                        right = 0x7f00 - ((balance * 0x2500) >> 14);
                    }
                    left = (left * volume) >> 15;
                    right = (right * volume) >> 15;
                } else {
                    right = (volume * 0x5a00) >> 15;
                    left = right;
                }
                channel->voice_attr.volume.left = left;
                channel->voice_attr.volume.right = right;
                channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_VOLUME;
            }
            if ((voice_update_flags & SUZUKI_VOICE_UPDATE_PITCH) != 0) {
                channel->voice_attr.pitch = suzuki_smd_calculate_pitch((channel->pitch >> 16)
                                                + channel->pitch_modulation + (music->pitch_shift.value >> 16))
                    & 0x3fff;
                channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_PITCH;
            }
            if ((voice_update_flags & SUZUKI_VOICE_UPDATE_KEY_ON) != 0 && (channel->flags & SUZUKI_CHANNEL_MUTED) == 0)
                key_on |= channel->voice_attr.voice;
            if ((voice_update_flags & SUZUKI_VOICE_UPDATE_KEY_OFF) != 0)
                key_off |= channel->voice_attr.voice;
            channel->voice_update_flags = 0;
        }
        channel++;
    } while (--channel_count != 0);
    music->key_on_mask |= key_on & allowed;
    music->key_off_mask |= key_off & allowed;
}

/* Converts a key in 1/256 semitones (key number << 8 | fine step) to an SPU
 * pitch: the octave-6 table value for the semitone and fine step, shifted by
 * the key's octave distance from octave 6. */
s16 suzuki_smd_calculate_pitch(s16 key) {
    s32 index = (key & 0x7fff) >> 8;
    s32 shift;
    s16 pitch;
    u8 fine_step = key;

    pitch = s_suzuki_smd_pitch_table[fine_step + (s_suzuki_smd_key_semitones[index] << 8)];
    shift = 6 - s_suzuki_smd_key_octaves[index];
    if (shift >= 0)
        pitch = pitch >> shift;
    else
        pitch = pitch << -shift;
    return pitch;
}

/* Steps the four modulators of each channel once per tick. An active
 * modulator first waits out delay, then calls its waveform step. While
 * the fade-in level fade_level is below 0x100 the result is scaled by
 * fade_level / 0x100 and fade_level grows by fade_step. The high half
 * of the result is added to the pitch, volume or balance modulation selected
 * by target, and voice_update_flags marks a pitch or volume/balance change
 * for the voice update.
 *
 * Iterating a copy of the channels parameter keeps the target's prologue
 * order (the count parameter is copied first). */
void suzuki_smd_update_modulators(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count) {
    suzuki_music_channel_t* channel = channels;
    suzuki_modulator_t* modulator;
    s32 i;
    s32 value;

    do {
        if (channel->flags != 0) {
            for (i = SUZUKI_MODULATOR_COUNT, modulator = channel->modulators; i != 0; modulator++, i--) {
                if ((modulator->flags & SUZUKI_MODULATOR_ACTIVE) == 0)
                    continue;
                if (modulator->delay != 0) {
                    modulator->delay--;
                    continue;
                }
                value = modulator->step(modulator);
                if (modulator->fade_level < 0x100) {
                    value = (value >> 8) * modulator->fade_level;
                    modulator->fade_level += modulator->fade_step;
                }
                value >>= 16;
                switch (modulator->target) {
                case SUZUKI_MODULATOR_TARGET_PITCH:
                    channel->pitch_modulation += value;
                    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_PITCH;
                    break;
                case SUZUKI_MODULATOR_TARGET_VOLUME:
                    channel->volume_modulation += value;
                    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
                    break;
                case SUZUKI_MODULATOR_TARGET_BALANCE:
                    channel->balance_modulation += value;
                    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
                    break;
                }
            }
        }
        channel++;
    } while (--channel_count != 0);
}

/* Deactivates a modulator. */
void suzuki_smd_modulator_deactivate(suzuki_modulator_t* modulator) {
    modulator->flags &= ~SUZUKI_MODULATOR_ACTIVE;
}

/* Modulator step: square wave between 0 and amplitude, toggling each time
 * the timer expires. Returns the current value. */
s32 suzuki_smd_modulator_step_square(suzuki_modulator_t* modulator) {
    if (--modulator->timer == 0) {
        modulator->timer = modulator->timer_reset;
        modulator->value = modulator->value == 0 ? modulator->amplitude : 0;
    }
    return modulator->value;
}

/* Modulator step: square wave between +amplitude and -amplitude, the sign
 * tracked in SUZUKI_MODULATOR_NEGATED. Returns the current value. */
s32 suzuki_smd_modulator_step_alternating(suzuki_modulator_t* modulator) {
    if (--modulator->timer == 0) {
        modulator->timer = modulator->timer_reset;
        modulator->value = (modulator->flags & SUZUKI_MODULATOR_NEGATED) ? -modulator->amplitude : modulator->amplitude;
        modulator->flags ^= SUZUKI_MODULATOR_NEGATED;
    }
    return modulator->value;
}

/* Modulator step: triangle wave; the increment flips sign
 * (SUZUKI_MODULATOR_NEGATED) each time the timer expires and is accumulated
 * into value every step. */
s32 suzuki_smd_modulator_step_triangle(suzuki_modulator_t* modulator) {
    s32 increment;

    if (--modulator->timer == 0) {
        increment = modulator->amplitude;
        modulator->timer = modulator->timer_reset;
        if ((modulator->flags & SUZUKI_MODULATOR_NEGATED) != 0)
            increment = -increment;
        modulator->increment = increment;
        modulator->flags ^= SUZUKI_MODULATOR_NEGATED;
    }
    modulator->value += modulator->increment;
    return modulator->value;
}

/* Modulator step: triangle wave centered on its start value. The first
 * half-period lasts timer_reset steps; SUZUKI_MODULATOR_HALF_PERIOD_DONE
 * then doubles every later one, so value swings between
 * -amplitude * timer_reset and +amplitude * timer_reset. The increment flips
 * sign (SUZUKI_MODULATOR_NEGATED) each time the timer expires. */
s32 suzuki_smd_modulator_step_triangle_centered(suzuki_modulator_t* modulator) {
    s32 amplitude;
    u16 flags;

    flags = modulator->flags;
    if (--modulator->timer == 0) {
        modulator->timer = modulator->timer_reset;
        modulator->timer *= (flags & SUZUKI_MODULATOR_HALF_PERIOD_DONE) ? 2 : 1;
        amplitude = modulator->amplitude;
        modulator->increment = amplitude;
        if ((flags & SUZUKI_MODULATOR_NEGATED) != 0)
            modulator->increment = -amplitude;
        flags = (flags | SUZUKI_MODULATOR_HALF_PERIOD_DONE) ^ SUZUKI_MODULATOR_NEGATED;
        modulator->flags = flags;
    }
    modulator->value += modulator->increment;
    return modulator->value;
}

/* Modulator step: sawtooth; value ramps by amplitude every step and resets
 * to zero when the timer expires. */
s32 suzuki_smd_modulator_step_sawtooth(suzuki_modulator_t* modulator) {
    if (--modulator->timer == 0) {
        modulator->value = 0;
        modulator->timer = modulator->timer_reset;
    } else {
        modulator->value += modulator->amplitude;
    }
    return modulator->value;
}

/* Modulator step: sample and hold. Each time the timer expires value takes a
 * new random multiple of amplitude >> 15 (0 to about amplitude). The generator
 * is also advanced on every step. */
s32 suzuki_smd_modulator_step_random(suzuki_modulator_t* modulator) {
    suzuki_smd_random();
    if (--modulator->timer == 0) {
        modulator->timer = modulator->timer_reset;
        modulator->value = (modulator->amplitude >> 15) * suzuki_smd_random();
    }
    return modulator->value;
}

/* Modulator step: bipolar sample and hold. Each time the timer expires value
 * takes a new random level between -amplitude and about +amplitude. */
s32 suzuki_smd_modulator_step_random_bipolar(suzuki_modulator_t* modulator) {
    if (--modulator->timer == 0) {
        modulator->timer = modulator->timer_reset;
        modulator->value = (modulator->amplitude >> 14) * suzuki_smd_random() - modulator->amplitude;
    }
    return modulator->value;
}

/* Seed the xorshift state that suzuki_smd_random steps (suzuki_sound_init seeds 0x12345678).
 * Nothing in the tree calls it. */
void suzuki_smd_seed_random(s32 value) {
    g_suzuki_smd_random_state = value;
}

/* xorshift step; returns the low 15 bits of the new state */
s32 suzuki_smd_random(void) {
    s32 state = g_suzuki_smd_random_state;

    state ^= state << 17;
    state ^= state >> 15;
    g_suzuki_smd_random_state = state;
    return state & 0x7fff;
}
