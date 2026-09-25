#include "fft/main.h"
#include "psx/types.h"

/* Per-tick channel pass run before the SMD interpreter. Steps the MUS tempo
 * ramp (opcode 0xA2) and rescales the tempo, then for every channel still
 * inside a note or rest (rest_length != 0) clears the modulation outputs,
 * steps the volume (flags_06 bit 3), pitch-slide (bit 0, held by bit 1) and
 * balance (bit 4) ramps, and counts down rest_length and note_ticks.
 * note_flags2 collects the resulting voice work: 0x100 volume/balance,
 * 0x200 pitch, 2 key off. When rest_length reaches 1 on a channel with
 * active bit 0x1000 the release time is forced to 6. */
void main_smd_update_channel_ramps(suzuki_music_t* music, suzuki_music_channel_t* channel, s16 channel_count) {
    u16 count;
    u16 active;
    u16 note_flags2;
    u16 flags;

    count = music->tempo_step_count;
    if (count != 0) {
        if (--count != 0)
            music->tempo.raw += music->tempo_step;
        else
            music->tempo.raw = music->tempo_target << 16;
        music->tempo_step_count = count;
        music->scaled_tempo = music->tempo.halves.high * (s16)(music->tempo_scale.value >> 16);
    }
    do {
        active = channel->active;
        if (active != 0) {
            note_flags2 = channel->note_flags2;
            flags = channel->flags_06;
            if (channel->rest_length != 0) {
                channel->pitch_modulation = 0;
                channel->volume_modulation = 0;
                channel->balance_modulation = 0;
                if ((flags & 8) != 0) {
                    note_flags2 |= 0x100;
                    if (--channel->volume_step_count == 0)
                        flags &= ~8;
                    channel->volume += channel->volume_step;
                }
                if ((flags & 1) != 0) {
                    note_flags2 |= 0x200;
                    if ((flags & 2) == 0 && --channel->pitch_slide_count == 0)
                        flags &= ~1;
                    channel->pitch_slide += channel->pitch_slide_step;
                }
                if ((flags & 0x10) != 0) {
                    note_flags2 |= 0x100;
                    if (--channel->balance_step_count == 0)
                        flags &= ~0x10;
                    channel->balance += channel->balance_step;
                }
                if (--channel->rest_length == 1 && (active & 0x1000) != 0) {
                    channel->release_time = 6;
                    channel->func_flags |= 0x80;
                }
                if ((active & 0x600) == 0 && --channel->note_ticks == 0) {
                    note_flags2 |= 2;
                    channel->active |= 0x400;
                }
            }
            channel->note_flags2 = note_flags2;
            channel->flags_06 = flags;
        }
        channel++;
    } while (--channel_count != 0);
}
