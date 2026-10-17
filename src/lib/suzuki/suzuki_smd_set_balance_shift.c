#include "fft/suzuki.h"
#include "psx/types.h"

/* Sets the music's balance shift, immediately when time is 0, otherwise as a ramp
 * over time ticks. */
void suzuki_smd_set_balance_shift(suzuki_music_t* music, s16 value, s16 time) {
    s32 difference;

    music->balance_shift.target = value << 8;
    if (time == 0) {
        music->balance_shift.value = value << 24;
        music->balance_shift.count = 0;
        suzuki_smd_set_voice_update_flags_all_channels(SUZUKI_VOICE_UPDATE_VOLUME, music);
    } else {
        difference = (value << 16) - (music->balance_shift.value >> 8);
        if (difference != 0) {
            music->balance_shift.count = time;
            music->balance_shift.step = (difference / time) << 8;
        }
    }
}
