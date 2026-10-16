#include "psx/suzuki.h"
#include "psx/types.h"

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
