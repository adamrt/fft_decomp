#include "fft/suzuki.h"
#include "psx/types.h"

/* Plays a sound effect on the SFX voice pair that starts at the even
 * channel at or below channel. */
void suzuki_sound_play_sfx_in_channel(s32 sound_id, s32 channel) {
    u16 first_channel = channel;

    first_channel &= ~1;
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, first_channel), sound_id, 0x6000, 0x4000);
    }
}
