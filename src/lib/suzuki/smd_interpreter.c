/* Decode music and sound-effect sequences a channel at a time. Notes select a
 * semitone and one of nineteen durations; commands set voice attributes, repeat
 * sections or trigger other sounds. Look-ahead follows repeats without changing
 * playback state so the current note can be tied to the next one. */

#include "fft/suzuki.h"
#include "psx/types.h"

/* Opcodes 0x80 through 0xff. */
static suzuki_smd_handler_t g_suzuki_smd_opcode_handlers[128] = {
    suzuki_smd_rest,                    /* 0x80 */
    suzuki_smd_fermata,                 /* 0x81 */
    suzuki_smd_no_instruction,          /* 0x82 */
    suzuki_smd_no_instruction,          /* 0x83 */
    suzuki_smd_no_instruction,          /* 0x84 */
    suzuki_smd_no_instruction,          /* 0x85 */
    suzuki_smd_no_instruction,          /* 0x86 */
    suzuki_smd_no_instruction,          /* 0x87 */
    suzuki_smd_no_instruction,          /* 0x88 */
    suzuki_smd_no_instruction,          /* 0x89 */
    suzuki_smd_opcode_8a,               /* 0x8a */
    suzuki_smd_no_instruction,          /* 0x8b */
    suzuki_smd_no_instruction,          /* 0x8c */
    suzuki_smd_loop_point_if_selected,  /* 0x8d */
    suzuki_smd_opcode_8e,               /* 0x8e */
    suzuki_smd_opcode_8f,               /* 0x8f */
    suzuki_smd_end_bar,                 /* 0x90 */
    suzuki_smd_loop,                    /* 0x91 */
    suzuki_smd_no_instruction,          /* 0x92 */
    suzuki_smd_no_instruction,          /* 0x93 */
    suzuki_smd_octave,                  /* 0x94 */
    suzuki_smd_raise_octave,            /* 0x95 */
    suzuki_smd_lower_octave,            /* 0x96 */
    suzuki_smd_time_signature,          /* 0x97 */
    suzuki_smd_repeat,                  /* 0x98 */
    suzuki_smd_coda,                    /* 0x99 */
    suzuki_smd_to_coda,                 /* 0x9a */
    suzuki_smd_no_instruction,          /* 0x9b */
    suzuki_smd_play_sfx,                /* 0x9c */
    suzuki_smd_stop_sfx,                /* 0x9d */
    suzuki_smd_play_vfx,                /* 0x9e */
    suzuki_smd_no_instruction,          /* 0x9f */
    suzuki_smd_tempo,                   /* 0xa0 */
    suzuki_smd_accelerando,             /* 0xa1 */
    suzuki_smd_tempo_ramp,              /* 0xa2 */
    suzuki_smd_no_instruction,          /* 0xa3 */
    suzuki_smd_opcode_a4,               /* 0xa4 */
    suzuki_smd_opcode_a5,               /* 0xa5 */
    suzuki_smd_master_volume,           /* 0xa6 */
    suzuki_smd_master_volume_ramp,      /* 0xa7 */
    suzuki_smd_no_instruction,          /* 0xa8 */
    suzuki_smd_gate_time,               /* 0xa9 */
    suzuki_smd_set_voice,               /* 0xaa */
    suzuki_smd_no_instruction,          /* 0xab */
    suzuki_smd_instrument,              /* 0xac */
    suzuki_smd_length_adjust,           /* 0xad */
    suzuki_smd_opcode_ae,               /* 0xae */
    suzuki_smd_opcode_af,               /* 0xaf */
    suzuki_smd_slur_on,                 /* 0xb0 */
    suzuki_smd_slur_off,                /* 0xb1 */
    suzuki_smd_enable_lfo,              /* 0xb2 */
    suzuki_smd_disable_lfo,             /* 0xb3 */
    suzuki_smd_enable_noise,            /* 0xb4 */
    suzuki_smd_increase_noise,          /* 0xb5 */
    suzuki_smd_turn_on_noise,           /* 0xb6 */
    suzuki_smd_disable_noise,           /* 0xb7 */
    suzuki_smd_acoustics,               /* 0xb8 */
    suzuki_smd_no_instruction,          /* 0xb9 */
    suzuki_smd_enable_reverb,           /* 0xba */
    suzuki_smd_disable_reverb,          /* 0xbb */
    suzuki_smd_no_instruction,          /* 0xbc */
    suzuki_smd_no_instruction,          /* 0xbd */
    suzuki_smd_no_instruction,          /* 0xbe */
    suzuki_smd_no_instruction,          /* 0xbf */
    suzuki_smd_naturale,                /* 0xc0 */
    suzuki_smd_envelope_shape,          /* 0xc1 */
    suzuki_smd_attack_time,             /* 0xc2 */
    suzuki_smd_decay_time,              /* 0xc3 */
    suzuki_smd_sustain_time,            /* 0xc4 */
    suzuki_smd_release,                 /* 0xc5 */
    suzuki_smd_sustain_level,           /* 0xc6 */
    suzuki_smd_envelope_tail,           /* 0xc7 */
    suzuki_smd_attack_mode,             /* 0xc8 */
    suzuki_smd_sustain_mode,            /* 0xc9 */
    suzuki_smd_release_mode,            /* 0xca */
    suzuki_smd_no_instruction,          /* 0xcb */
    suzuki_smd_no_instruction,          /* 0xcc */
    suzuki_smd_no_instruction,          /* 0xcd */
    suzuki_smd_no_instruction,          /* 0xce */
    suzuki_smd_no_instruction,          /* 0xcf */
    suzuki_smd_pitch_offset,            /* 0xd0 */
    suzuki_smd_add_pitch_offset_coarse, /* 0xd1 */
    suzuki_smd_add_pitch_offset_fine,   /* 0xd2 */
    suzuki_smd_add_pitch_offset_wide,   /* 0xd3 */
    suzuki_smd_pitch_slide,             /* 0xd4 */
    suzuki_smd_toggle_pitch_slide_hold, /* 0xd5 */
    suzuki_smd_portamento,              /* 0xd6 */
    suzuki_smd_pitch_shift_fade_in,     /* 0xd7 */
    suzuki_smd_pitch_shift,             /* 0xd8 */
    suzuki_smd_pitch_shift_waveform,    /* 0xd9 */
    suzuki_smd_pitch_shift_on,          /* 0xda */
    suzuki_smd_pitch_shift_off,         /* 0xdb */
    suzuki_smd_pitch_slide_off,         /* 0xdc */
    suzuki_smd_no_instruction,          /* 0xdd */
    suzuki_smd_no_instruction,          /* 0xde */
    suzuki_smd_no_instruction,          /* 0xdf */
    suzuki_smd_dynamic,                 /* 0xe0 */
    suzuki_smd_crescendo,               /* 0xe1 */
    suzuki_smd_fermata_ramp,            /* 0xe2 */
    suzuki_smd_tremolo_fade_in,         /* 0xe3 */
    suzuki_smd_tremolo,                 /* 0xe4 */
    suzuki_smd_tremolo_waveform,        /* 0xe5 */
    suzuki_smd_tremolo_on,              /* 0xe6 */
    suzuki_smd_tremolo_off,             /* 0xe7 */
    suzuki_smd_balance,                 /* 0xe8 */
    suzuki_smd_shift_balance,           /* 0xe9 */
    suzuki_smd_balance_ramp,            /* 0xea */
    suzuki_smd_pan_lfo_fade_in,         /* 0xeb */
    suzuki_smd_pan_lfo,                 /* 0xec */
    suzuki_smd_pan_lfo_waveform,        /* 0xed */
    suzuki_smd_pan_lfo_on,              /* 0xee */
    suzuki_smd_pan_lfo_off,             /* 0xef */
    suzuki_smd_modulator_select,        /* 0xf0 */
    suzuki_smd_modulator_set_depth,     /* 0xf1 */
    suzuki_smd_modulator_set_fade_in,   /* 0xf2 */
    suzuki_smd_no_instruction,          /* 0xf3 */
    suzuki_smd_no_instruction,          /* 0xf4 */
    suzuki_smd_modulator_enable_mask,   /* 0xf5 */
    suzuki_smd_modulator_enable,        /* 0xf6 */
    suzuki_smd_modulator_disable,       /* 0xf7 */
    suzuki_smd_no_instruction,          /* 0xf8 */
    suzuki_smd_no_instruction,          /* 0xf9 */
    suzuki_smd_no_instruction,          /* 0xfa */
    suzuki_smd_no_instruction,          /* 0xfb */
    suzuki_smd_no_instruction,          /* 0xfc */
    suzuki_smd_scale_tempo,             /* 0xfd */
    suzuki_smd_select_sound_font,       /* 0xfe */
    suzuki_smd_end_track,               /* 0xff */
};

/* Bytes skipped by look-ahead, including the opcode. Stop SFX retains the
 * original extra parameter byte; its handler consumes only two. */
static u8 g_suzuki_smd_opcode_lengths[128] = { 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 4, 1, 1, 1, 0, 0, 2, 1, 1, 3,
    2, 1, 1, 0, 4, 4, 4, 0, 2, 2, 3, 0, 2, 2, 2, 3, 0, 2, 2, 0, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 4, 0, 1, 1, 0, 0, 0,
    0, 1, 4, 2, 2, 2, 2, 2, 3, 2, 2, 2, 0, 0, 0, 0, 0, 2, 2, 2, 3, 3, 1, 2, 2, 4, 4, 1, 1, 1, 0, 0, 0, 2, 2, 3, 2, 4, 4,
    1, 1, 2, 2, 3, 2, 4, 4, 1, 1, 4, 4, 3, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 2, 2, 1 };

/* The nineteen note lengths repeat for each semitone. Zero reads an explicit
 * duration from the stream; 192 ticks is a whole note. */
#define NOTE_DURATIONS 0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2
static u8 g_suzuki_smd_note_durations[228]
    = { NOTE_DURATIONS, NOTE_DURATIONS, NOTE_DURATIONS, NOTE_DURATIONS, NOTE_DURATIONS, NOTE_DURATIONS, NOTE_DURATIONS,
          NOTE_DURATIONS, NOTE_DURATIONS, NOTE_DURATIONS, NOTE_DURATIONS, NOTE_DURATIONS };
#undef NOTE_DURATIONS

/* Each run of nineteen note bytes names the same semitone. */
#define NOTE_SEMITONE(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
static u8 g_suzuki_smd_note_semitones[228] = { NOTE_SEMITONE(0), NOTE_SEMITONE(1), NOTE_SEMITONE(2), NOTE_SEMITONE(3),
    NOTE_SEMITONE(4), NOTE_SEMITONE(5), NOTE_SEMITONE(6), NOTE_SEMITONE(7), NOTE_SEMITONE(8), NOTE_SEMITONE(9),
    NOTE_SEMITONE(10), NOTE_SEMITONE(11) };
#undef NOTE_SEMITONE

/* Reads the SMD events of channel_count consecutive channels, called once
 * per tick.
 *
 * A channel whose event_ticks has run out runs opcode handlers from
 * g_suzuki_smd_opcode_handlers until a note, Rest or Fermata sets
 * SUZUKI_CHANNEL_SOUNDING or SUZUKI_CHANNEL_KEYED_OFF. A byte below
 * SUZUKI_SMD_OPCODE_FIRST is a note's velocity; the next byte indexes the
 * note tables for the key and a duration (0 reads an explicit length byte).
 * The reader then looks ahead without changing the channel: it follows End
 * Bar and the repeat opcodes (Coda, To Coda) on a local view of the repeat
 * stack and skips other opcodes by g_suzuki_smd_opcode_lengths, so
 * SUZUKI_CHANNEL_NOTE_FOLLOWS records whether a note comes next; a Rest or
 * Slur On/Off clears SUZUKI_CHANNEL_TIED and a Fermata sets it. gate_time
 * scales the length into note_ticks in sixteenths. On key-on (the previous
 * event left the channel keyed off) a key change under
 * SUZUKI_RAMP_PORTAMENTO starts a slide from the previous key over
 * portamento_ticks ticks, and the enabled modulators restart. voice_bit
 * follows the channel for music->channel_mask.
 *
 * Matching notes: channel is a local copy of the parameter (the target
 * copies $a2 before $a1). op also carries the key, because the target
 * zero-extends it again before the shift. Every store through channel ends
 * CSE's copy of a field, so the length_adjust and pitch_slide_count stores come
 * before the reloads of event_ticks and ramp_flags that the target performs;
 * mod_flags keeps the single flags load the target tests three times. */
void suzuki_smd_read_instructions(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count) {
    suzuki_music_channel_t* channel = channels;
    u32 voice_bit = 1;
    s32 key_on;
    u16 flags;
    u8* note_data;
    u16 op;
    u16 note;
    s16 length;
    u16 ticks;
    suzuki_repeat_t* repeat;
    suzuki_modulator_t* modulator;
    s32 i;
    s32 diff;
    u16 mod_flags;

    do {
        if (channel->flags != 0) {
            key_on = 0;
            if (channel->event_ticks == 0) {
                flags = channel->flags;
                note_data = channel->note_data;
                channel->flags = flags & ~(SUZUKI_CHANNEL_KEYED_OFF | SUZUKI_CHANNEL_TIED | SUZUKI_CHANNEL_SOUNDING);
                do {
                    op = *note_data++;
                    if (op < SUZUKI_SMD_OPCODE_FIRST) {
                        if (!(channel->flags & SUZUKI_CHANNEL_SFX)) {
                            channel->velocity = op << 8;
                        }
                        channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
                        note = *note_data++;
                        op = channel->key = channel->octave_base + g_suzuki_smd_note_semitones[note];
                        note = g_suzuki_smd_note_durations[note];
                        if (note != 0) {
                            channel->event_ticks = note;
                        } else {
                            channel->event_ticks = *note_data++;
                        }
                        channel->voice_attr.rr = channel->note_release_rate;
                        channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_RELEASE;
                        channel->pitch = ((op << 8) + channel->pitch_offset + channel->fine_tune) << 16;
                        channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_PITCH;
                        channel->flags |= SUZUKI_CHANNEL_SOUNDING | SUZUKI_CHANNEL_NOTE_READ;
                        if (flags & SUZUKI_CHANNEL_KEYED_OFF) {
                            key_on = 1;
                            channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_KEY_ON;
                        }
                        if (channel->flags & SUZUKI_CHANNEL_INSTRUMENT_PENDING) {
                            channel->flags &= ~SUZUKI_CHANNEL_INSTRUMENT_PENDING;
                            channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME | SUZUKI_VOICE_UPDATE_PITCH;
                            channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_ALL;
                        }
                    } else {
                        op -= SUZUKI_SMD_OPCODE_FIRST;
                        note_data = g_suzuki_smd_opcode_handlers[op](note_data, music, channel);
                        if (channel->flags == 0) {
                            music->channel_mask &= ~voice_bit;
                            break;
                        }
                    }
                } while (!(channel->flags & (SUZUKI_CHANNEL_KEYED_OFF | SUZUKI_CHANNEL_SOUNDING)));
                channel->note_data = note_data;
                if (channel->flags & SUZUKI_CHANNEL_SLUR) {
                    channel->flags |= SUZUKI_CHANNEL_TIED;
                }
                repeat = &channel->repeats[channel->repeat_depth];
                while ((op = *note_data) >= SUZUKI_SMD_OPCODE_FIRST) {
                    if (op == SUZUKI_SMD_OPCODE_END_BAR) {
                        note_data = channel->loop_note_data;
                        if (note_data != 0) {
                            continue;
                        }
                        break;
                    }
                    if (op == SUZUKI_SMD_OPCODE_REST) {
                        channel->flags &= ~SUZUKI_CHANNEL_TIED;
                        break;
                    }
                    if (op == SUZUKI_SMD_OPCODE_FERMATA) {
                        channel->flags |= SUZUKI_CHANNEL_TIED;
                        break;
                    }
                    if (op == SUZUKI_SMD_OPCODE_SLUR_ON || op == SUZUKI_SMD_OPCODE_SLUR_OFF) {
                        channel->flags &= ~SUZUKI_CHANNEL_TIED;
                        break;
                    }
                    if (op == SUZUKI_SMD_OPCODE_CODA) {
                        if (repeat->count != 0) {
                            note_data = repeat->start;
                            continue;
                        }
                        repeat--;
                    }
                    if (op == SUZUKI_SMD_OPCODE_TO_CODA && repeat->count == 0) {
                        note_data = repeat->coda;
                        repeat--;
                    } else {
                        op -= SUZUKI_SMD_OPCODE_FIRST;
                        note_data += g_suzuki_smd_opcode_lengths[op];
                    }
                }
                if (op < SUZUKI_SMD_OPCODE_FIRST) {
                    channel->flags |= SUZUKI_CHANNEL_NOTE_FOLLOWS;
                } else {
                    channel->flags &= ~SUZUKI_CHANNEL_NOTE_FOLLOWS;
                }
                length = channel->length_adjust + channel->event_ticks;
                if (length <= 0) {
                    channel->length_adjust += channel->event_ticks;
                    length += channel->event_ticks;
                }
                switch (channel->gate_time) {
                case 0x10:
                    ticks = length;
                    break;
                case 0xf:
                    ticks = length - 1;
                    if (ticks == 0) {
                        ticks = 1;
                    }
                    break;
                default:
                    ticks = (length * channel->gate_time) >> 4;
                    if (ticks == 0) {
                        ticks = 1;
                    }
                    break;
                }
                channel->note_ticks = ticks;
                channel->event_ticks = length;
                if (key_on) {
                    if (channel->ramp_flags & SUZUKI_RAMP_PORTAMENTO) {
                        diff = (channel->key - channel->previous_key) << 24;
                        if (diff != 0) {
                            channel->pitch_slide_count = channel->portamento_ticks;
                            channel->ramp_flags |= SUZUKI_RAMP_PITCH_SLIDE;
                            channel->pitch = ((channel->previous_key << 8) + channel->pitch_offset + channel->fine_tune)
                                << 16;
                            channel->pitch_slide_step = diff / channel->portamento_ticks;
                        }
                    }
                    channel->previous_key = channel->key;
                    modulator = channel->modulators;
                    for (i = SUZUKI_MODULATOR_COUNT; i != 0; i--) {
                        mod_flags = modulator->flags;
                        if (mod_flags & SUZUKI_MODULATOR_ACTIVE) {
                            if (mod_flags & SUZUKI_MODULATOR_RESTART_ON_KEY_ON) {
                                modulator->value = 0;
                                modulator->timer = 1;
                                modulator->delay = modulator->delay_reset;
                                modulator->fade_level = modulator->fade_step;
                                if (mod_flags & (SUZUKI_MODULATOR_RESTART_ON_KEY_ON | SUZUKI_MODULATOR_ACTIVE)) {
                                    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
                                }
                                modulator->flags &= ~(SUZUKI_MODULATOR_NEGATED | SUZUKI_MODULATOR_HALF_PERIOD_DONE);
                            }
                        }
                        modulator++;
                    }
                }
            }
        }
        channel++;
        voice_bit <<= 1;
    } while (--channel_count != 0);
}

/* Empty routine between the SMD reader and the opcode handlers; no opcode
 * table slot or call references it. */
void suzuki_smd_noop_8(void) {
}

/* Handler of the 37 unassigned SMD opcode slots: consumes nothing. */
u8* suzuki_smd_no_instruction(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    return note_data;
}

/* SMD opcode 0x80 "Rest": keys the channel off and waits the given number
 * of ticks. */
u8* suzuki_smd_rest(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->event_ticks = note_data[0];
    channel->flags |= SUZUKI_CHANNEL_KEYED_OFF;
    channel->voice_update_flags |= SUZUKI_VOICE_UPDATE_KEY_OFF;
    return note_data + 1;
}

/* SMD opcode 0x81 "Fermata": holds the sounding note for the given number
 * of ticks without keying it on again. */
u8* suzuki_smd_fermata(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 value = *note_data++;

    channel->flags |= SUZUKI_CHANNEL_SOUNDING;
    channel->event_ticks = value;
    return note_data;
}

/* SMD opcode 0x8A "Nop": a separate copy of suzuki_smd_no_instruction, with
 * length 1 in g_suzuki_smd_opcode_lengths. */
u8* suzuki_smd_opcode_8a(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    return note_data;
}

/* SMD opcode 0x8D "Loop Select": sets the loop point like Loop (0x91), but
 * only when its byte equals music->loop_select. */
u8* suzuki_smd_loop_point_if_selected(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 selector = *note_data++;

    if (selector == music->loop_select) {
        channel->loop_note_data = note_data;
        channel->loop_octave_base = channel->octave_base;
    }
    return note_data;
}

/* SMD opcode 0x8E "Skip 3": ignores its three parameter bytes. */
u8* suzuki_smd_opcode_8e(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    return note_data + 3;
}

/* SMD opcode 0x8F "Nop": a separate copy of suzuki_smd_no_instruction, with
 * length 1 in g_suzuki_smd_opcode_lengths. */
u8* suzuki_smd_opcode_8f(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    return note_data;
}

/* SMD opcode 0x90 "End Bar": jumps back to the loop point set by Loop
 * (0x91), counting the pass and restoring the octave. Without a loop
 * point the channel ends: its SPU voice is queued for key-off (the SFX MUS
 * record also drops the voice from its LFO, noise and reverb masks and from
 * the SFX reservation, and marks envelope and LFO parameters for update),
 * and the read position stays on this opcode. */
u8* suzuki_smd_end_bar(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 mask;

    if (channel->loop_note_data) {
        note_data = channel->loop_note_data;
        channel->loop_count++;
        channel->octave_base = channel->loop_octave_base;
    } else {
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

/* SMD opcode 0x91 "Loop": marks the loop point End Bar (0x90) jumps to,
 * saving the note-data position and the current octave base. */
u8* suzuki_smd_loop(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->loop_note_data = note_data;
    channel->loop_octave_base = channel->octave_base;
    return note_data;
}

/* SMD opcode 0x94 "Octave": twelve semitones per octave. */
u8* suzuki_smd_octave(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->octave_base = note_data[0] * 12;
    return note_data + 1;
}

/* SMD opcode 0x95 "Raise Octave": raises the octave base by 12
 * semitones. */
u8* suzuki_smd_raise_octave(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->octave_base += 12;
    return note_data;
}

/* SMD opcode 0x96 "Lower Octave": lowers the octave base by 12
 * semitones. */
u8* suzuki_smd_lower_octave(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    channel->octave_base -= 12;
    return note_data;
}

/* SMD opcode 0x97 "Time Signature": numerator then denominator. They only
 * drive the music's bar and beat counters.
 * The numerator is held as a 16-bit value: `lbu` already zero-extends it,
 * so a u8 temporary would be masked again before the 16-bit stores. */
u8* suzuki_smd_time_signature(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 numerator = note_data[0];
    u8 denominator = note_data[1];

    music->ticks_per_beat = 0xc0 / (denominator * music->tick_divisor);
    music->beat_unit = denominator;
    music->beats_per_bar = numerator;
    music->beats_per_bar_copy = numerator;
    music->beat_ticks_remaining = music->ticks_per_beat;
    return note_data + 2;
}

/* SMD opcode 0xA4: sets the song-wide byte _unknown_01c, which nothing
 * reads. */
u8* suzuki_smd_opcode_a4(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->_unknown_01c = *note_data;
    return note_data + 1;
}

/* SMD opcode 0xA5: adds a delta to the song-wide byte
 * that opcode 0xA4 sets. */
u8* suzuki_smd_opcode_a5(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    music->_unknown_01c += *note_data;
    return note_data + 1;
}

/* SMD opcode 0x98 "Repeat": pushes a repeat entry that plays the
 * following section as many times as its byte gives, remembering the start
 * and the octave. */
u8* suzuki_smd_repeat(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_repeat_t* repeat;

    channel->repeat_depth++;
    repeat = &channel->repeats[channel->repeat_depth];
    repeat->count = *note_data++ - 1;
    repeat->start = note_data;
    repeat->octave_base = channel->octave_base;
    return note_data;
}

/* SMD opcode 0x99 "Coda": ends a repeated section. While passes
 * remain it records this position and octave for To Coda (0x9A) and jumps
 * back to the section start; after the last pass it pops the entry. */
u8* suzuki_smd_coda(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_repeat_t* repeat = &channel->repeats[channel->repeat_depth];

    if (--repeat->count != 0xff) {
        repeat->coda = note_data;
        repeat->coda_octave = channel->octave_base;
        note_data = repeat->start;
        channel->octave_base = repeat->octave_base;
    } else {
        channel->repeat_depth--;
    }
    return note_data;
}

/* SMD opcode 0x9A "To Coda": on the last pass of a repeated section
 * jumps past its Coda (0x99), restoring the octave recorded there, and pops
 * the entry. */
u8* suzuki_smd_to_coda(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_repeat_t* repeat = &channel->repeats[channel->repeat_depth];

    if (repeat->count == 0) {
        note_data = repeat->coda;
        channel->octave_base = repeat->coda_octave;
        channel->repeat_depth--;
    }
    return note_data;
}

/* SMD opcode 0x9C: play a sound effect. Little-endian sound id, then a
 * volume byte; the balance is centered. */
u8* suzuki_smd_play_sfx(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_sound_play_sfx_with_settings_find_channel(note_data[0] | (note_data[1] << 8), note_data[2], 0x40);
    return note_data + 3;
}

/* SMD opcode 0x9D: stop the sound effect with the little-endian sound id.
 * It consumes two parameter bytes; g_suzuki_smd_opcode_lengths gives it three. */
u8* suzuki_smd_stop_sfx(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_sound_stop_sfx_channels(note_data[0] | (note_data[1] << 8));
    return note_data + 2;
}

/* SMD opcode 0x9E "Play VFX": moves the channel into a loaded sound
 * resource. The resource is the one whose id matches the high half
 * of the channel's sound id (id 0 takes the list head). Its channel_offsets
 * entry sound * 2 + byte 2, with the sound number little-endian in bytes 0-1,
 * gives the stream, and reading resumes three bytes past it. Without a matching resource
 * the read position is returned unchanged, so the parameter bytes are read
 * next as note data. */
u8* suzuki_smd_play_vfx(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_sound_resource_t* resource = g_suzuki_sound_resource_list;
    s16 sound = note_data[0] | (note_data[1] << 8);
    s16 id = channel->sound_id >> 16;
    u8 index = note_data[2];

    if (id != 0) {
        while (resource->id != id) {
            resource = resource->next;
            if (resource == 0) {
                return note_data;
            }
        }
    }
    note_data = (u8*)resource + resource->channel_offsets[sound * 2 + index];
    return note_data + 3;
}
