#include "fft/suzuki.h"
#include "psx/types.h"

/* Plays a sound on SFX channels 6-7 at the default volume (0x60) and center
 * balance, with a priority suzuki_sound_find_sfx_channel never steals. */
void suzuki_sound_play_direct_sfx(s32 sound) {
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_DIRECT, 6), sound, 0x6000, 0x4000);
    }
}
