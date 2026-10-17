#include "fft/suzuki.h"
#include "psx/types.h"

/* suzuki_sound_play_sfx_in_channel with explicit volume and balance bytes. */
void suzuki_sound_play_sfx_with_settings_in_channel(s32 sound_id, s32 channel, s32 volume, s32 balance) {
    u16 first_channel = channel;

    first_channel &= ~1;
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_SFX_ENABLED) != 0) {
        g_suzuki_sound_sfx_channel_count = 2;
        suzuki_sound_start_sfx(SUZUKI_SOUND_SFX_CHANNEL_ID(SUZUKI_SOUND_SFX_PRIORITY_STEALABLE, first_channel),
            sound_id, volume << 8, balance << 8);
    }
}
