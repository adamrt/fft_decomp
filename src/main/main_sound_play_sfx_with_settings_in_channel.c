#include "fft/main.h"
#include "psx/types.h"

/* main_sound_play_sfx_in_channel with explicit volume and balance bytes. */
void main_sound_play_sfx_with_settings_in_channel(s32 sound_id, s32 channel, s32 volume, s32 balance) {
    channel &= 0xfffe;
    if ((g_main_sound_driver_flags & 0x1000) != 0) {
        g_main_sound_sfx_channel_count = 2;
        main_sound_start_sfx(channel | 0x2000, sound_id, volume << 8, balance << 8);
    }
}
