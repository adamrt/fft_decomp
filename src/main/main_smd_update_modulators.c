#include "fft/main_sound.h"
#include "psx/types.h"

/* Steps the four modulators of each channel once per tick. An active
 * modulator first waits out counter_14, then calls its waveform step. While
 * the fade-in level counter_18 is below 0x100 the result is scaled by
 * counter_18 / 0x100 and counter_18 grows by counter_18_reset. The high half
 * of the result is added to the pitch, volume or balance modulation selected
 * by target, and note_flags2 marks a pitch (0x200) or volume/balance (0x100)
 * change for the voice update.
 *
 * Iterating a copy of the channels parameter keeps the target's prologue
 * order (the count parameter is copied first). */
void main_smd_update_modulators(suzuki_music_t* music, suzuki_music_channel_t* channels, s16 channel_count) {
    suzuki_music_channel_t* channel;
    suzuki_modulator_t* modulator;
    s32 i;
    s32 value;

    channel = channels;
    do {
        if (channel->active != 0) {
            for (i = 4, modulator = channel->modulators; i != 0; modulator++, i--) {
                if ((modulator->flags & 1) == 0)
                    continue;
                if (modulator->counter_14 != 0) {
                    modulator->counter_14--;
                    continue;
                }
                value = modulator->step(modulator);
                if (modulator->counter_18 < 0x100) {
                    value = (value >> 8) * modulator->counter_18;
                    modulator->counter_18 += modulator->counter_18_reset;
                }
                value >>= 16;
                switch (modulator->target) {
                case 0:
                    channel->pitch_modulation += value;
                    channel->note_flags2 |= 0x200;
                    break;
                case 1:
                    channel->volume_modulation += value;
                    channel->note_flags2 |= 0x100;
                    break;
                case 2:
                    channel->balance_modulation += value;
                    channel->note_flags2 |= 0x100;
                    break;
                }
            }
        }
        channel++;
    } while (--channel_count != 0);
}
