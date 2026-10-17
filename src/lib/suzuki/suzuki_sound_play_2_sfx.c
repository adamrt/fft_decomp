#include "fft/suzuki.h"
#include "psx/types.h"

/* Plays two sound effects on the SFX voice pairs 0 and 2. */
void suzuki_sound_play_2_sfx(s32 first, s32 second) {
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, 0), first, 0x6000, 0x4000);
        suzuki_sound_start_sfx(
            SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, 2), second, 0x6000, 0x4000);
    }
}
