#include "fft/suzuki.h"
#include "psx/types.h"

/* Per-tick channel pass run before the SMD interpreter. Steps the MUS tempo
 * ramp (Tempo Ramp) and rescales the tempo, then for every channel still
 * inside a note or rest (event_ticks != 0) clears the modulation outputs,
 * steps the volume, pitch-slide (unless held) and balance ramps that
 * ramp_flags marks, and counts down event_ticks and note_ticks.
 * voice_update_flags collects the resulting voice work: volume/balance,
 * pitch and key off. When event_ticks reaches 1 on a channel whose next
 * event is a note (SUZUKI_CHANNEL_NOTE_FOLLOWS) the release rate is forced
 * to 6, so the voice has faded by the key-on. */
void suzuki_smd_update_channel_ramps(suzuki_music_t* music, suzuki_music_channel_t* channel, s16 channel_count) {
    u16 count = music->tempo_step_count;
    u16 active;
    u16 voice_update_flags;
    u16 flags;

    if (count != 0) {
        if (--count != 0)
            music->tempo += music->tempo_step;
        else
            music->tempo = music->tempo_target << 16;
        music->tempo_step_count = count;
        music->scaled_tempo = (music->tempo >> 16) * (music->tempo_scale.value >> 16);
    }
    do {
        active = channel->flags;
        if (active != 0) {
            voice_update_flags = channel->voice_update_flags;
            flags = channel->ramp_flags;
            if (channel->event_ticks != 0) {
                channel->pitch_modulation = 0;
                channel->volume_modulation = 0;
                channel->balance_modulation = 0;
                if ((flags & SUZUKI_RAMP_VOLUME) != 0) {
                    voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
                    if (--channel->volume_step_count == 0)
                        flags &= ~SUZUKI_RAMP_VOLUME;
                    channel->volume += channel->volume_step;
                }
                if ((flags & SUZUKI_RAMP_PITCH_SLIDE) != 0) {
                    voice_update_flags |= SUZUKI_VOICE_UPDATE_PITCH;
                    if ((flags & SUZUKI_RAMP_PITCH_SLIDE_HOLD) == 0 && --channel->pitch_slide_count == 0)
                        flags &= ~SUZUKI_RAMP_PITCH_SLIDE;
                    channel->pitch += channel->pitch_slide_step;
                }
                if ((flags & SUZUKI_RAMP_BALANCE) != 0) {
                    voice_update_flags |= SUZUKI_VOICE_UPDATE_VOLUME;
                    if (--channel->balance_step_count == 0)
                        flags &= ~SUZUKI_RAMP_BALANCE;
                    channel->balance += channel->balance_step;
                }
                if (--channel->event_ticks == 1 && (active & SUZUKI_CHANNEL_NOTE_FOLLOWS) != 0) {
                    channel->voice_attr.rr = 6;
                    channel->voice_attr_flags |= SUZUKI_VOICE_ATTR_RELEASE;
                }
                if ((active & (SUZUKI_CHANNEL_KEYED_OFF | SUZUKI_CHANNEL_TIED)) == 0 && --channel->note_ticks == 0) {
                    voice_update_flags |= SUZUKI_VOICE_UPDATE_KEY_OFF;
                    channel->flags |= SUZUKI_CHANNEL_KEYED_OFF;
                }
            }
            channel->voice_update_flags = voice_update_flags;
            channel->ramp_flags = flags;
        }
        channel++;
    } while (--channel_count != 0);
}
