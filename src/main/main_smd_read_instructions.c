#include "fft/main.h"
#include "psx/types.h"

/* Reads the SMD events of channel_count consecutive channels, called once
 * per tick.
 *
 * A channel whose rest_length has run out runs opcode handlers from
 * g_main_smd_opcode_handlers until a note or rest sets active bit 0x100 or
 * 0x400. A note byte below 0x80 is the velocity; the next byte indexes the
 * note tables for the key and a duration (0 reads an explicit length byte).
 * The reader then looks ahead without changing the channel: it follows End
 * Bar & Loop (0x90) and the repeat opcodes (0x99, 0x9A) on a local copy of
 * the repeat stack and skips other opcodes by g_main_smd_opcode_lengths, so
 * active bit 0x1000 records whether a note comes next; a rest (0x80) or
 * 0xB0/0xB1 clears bit 0x200 and a fermata (0x81) sets it. gate_time scales
 * the length into note_ticks in sixteenths. On key-on (the previous event
 * ended, active 0x400) a key change with flags_06 bit 2 set (opcode 0xD6)
 * starts a slide from the previous key over portamento_ticks ticks, and the enabled
 * modulators restart. voice_bit follows the channel for music->channel_mask.
 *
 * Matching notes: channel is a local copy of the parameter (the target
 * copies $a2 before $a1). op also carries the key, because the target
 * zero-extends it again before the shift. Every store through channel ends
 * CSE's copy of a field, so the length_adjust and pitch_slide_count stores come
 * before the reloads of rest_length and flags_06 that the target performs;
 * mod_flags keeps the single flags load the target tests three times. */
void main_smd_read_instructions(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count) {
    suzuki_music_channel_t* channel;
    u32 voice_bit;
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

    channel = channels;
    voice_bit = 1;
    do {
        if (channel->active != 0) {
            key_on = 0;
            if (channel->rest_length == 0) {
                flags = channel->active;
                note_data = channel->note_data;
                channel->active = flags & 0xf8ff;
                do {
                    op = *note_data++;
                    if (op < 0x80) {
                        if (!(channel->active & 8)) {
                            channel->velocity = op << 8;
                        }
                        channel->note_flags2 |= 0x100;
                        note = *note_data++;
                        op = channel->key = channel->octave_base + g_main_smd_note_semitones[note];
                        note = g_main_smd_note_durations[note];
                        if (note != 0) {
                            channel->rest_length = note;
                        } else {
                            channel->rest_length = *note_data++;
                        }
                        channel->release_time = channel->release_2e;
                        channel->func_flags |= 0x80;
                        channel->pitch_slide = ((op << 8) + channel->pitch_offset + channel->fine_tune) << 16;
                        channel->note_flags2 |= 0x200;
                        channel->active |= 0x180;
                        if (flags & 0x400) {
                            key_on = 1;
                            channel->note_flags2 |= 1;
                        }
                        if (channel->active & 0x8000) {
                            channel->active &= 0x7fff;
                            channel->note_flags2 |= 0x300;
                            channel->func_flags |= 0x1ff;
                        }
                    } else {
                        op -= 0x80;
                        note_data = g_main_smd_opcode_handlers[op](note_data, music, channel);
                        if (channel->active == 0) {
                            music->channel_mask &= ~voice_bit;
                            break;
                        }
                    }
                } while (!(channel->active & 0x500));
                channel->note_data = note_data;
                if (channel->active & 0x800) {
                    channel->active |= 0x200;
                }
                repeat = &channel->repeats[channel->repeat_depth];
                while ((op = *note_data) >= 0x80) {
                    if (op == 0x90) {
                        note_data = channel->loop_note_data;
                        if (note_data != 0) {
                            continue;
                        }
                        break;
                    }
                    if (op == 0x80) {
                        channel->active &= ~0x200;
                        break;
                    }
                    if (op == 0x81) {
                        channel->active |= 0x200;
                        break;
                    }
                    if (op == 0xb0 || op == 0xb1) {
                        channel->active &= ~0x200;
                        break;
                    }
                    if (op == 0x99) {
                        if (repeat->count != 0) {
                            note_data = repeat->start;
                            continue;
                        }
                        repeat--;
                    }
                    if (op == 0x9a && repeat->count == 0) {
                        note_data = repeat->coda;
                        repeat--;
                    } else {
                        op -= 0x80;
                        note_data += g_main_smd_opcode_lengths[op];
                    }
                }
                if (op < 0x80) {
                    channel->active |= 0x1000;
                } else {
                    channel->active &= ~0x1000;
                }
                length = (s8)channel->length_adjust + channel->rest_length;
                if (length <= 0) {
                    channel->length_adjust += channel->rest_length;
                    length += channel->rest_length;
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
                channel->rest_length = length;
                if (key_on) {
                    if (channel->flags_06 & 4) {
                        diff = (channel->key - channel->previous_key) << 24;
                        if (diff != 0) {
                            channel->pitch_slide_count = channel->portamento_ticks;
                            channel->flags_06 |= 1;
                            channel->pitch_slide
                                = ((channel->previous_key << 8) + channel->pitch_offset + channel->fine_tune) << 16;
                            channel->pitch_slide_step = diff / channel->portamento_ticks;
                        }
                    }
                    channel->previous_key = channel->key;
                    modulator = channel->modulators;
                    for (i = 4; i != 0; i--) {
                        mod_flags = modulator->flags;
                        if (mod_flags & 1) {
                            if (mod_flags & 2) {
                                modulator->value = 0;
                                modulator->timer = 1;
                                modulator->counter_14 = modulator->counter_14_reset;
                                modulator->counter_18 = modulator->counter_18_reset;
                                if (mod_flags & 3) {
                                    channel->note_flags2 |= 0x100;
                                }
                                modulator->flags &= ~0xc;
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
