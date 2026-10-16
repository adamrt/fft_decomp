#include "psx/suzuki.h"
#include "psx/types.h"

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
