#include "fft/main_sound.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Sets the master volume, at once or as a ramp of time ticks.
 *
 * The root-counter handler steps music_ramp and applies it to common.mvol;
 * a ramp is only started when the 8.8 volume actually changes. */
void main_sound_set_master_volume(s16 volume, s16 time) {
    s32 difference;

    g_main_sound_spu_state.music_ramp.target = volume;
    if (time == 0) {
        g_main_sound_spu_state.music_ramp.value = volume << 16;
        g_main_sound_spu_state.music_ramp.count = 0;
        g_main_sound_spu_state.music_volume = volume;
        main_sound_set_vol_balance(volume, &g_main_sound_spu_state.common.mvol, 0);
        g_main_sound_spu_state.common.mask |= 3;
    } else {
        difference = ((volume << 16) >> 8) - (g_main_sound_spu_state.music_ramp.value >> 8);
        if (difference != 0) {
            g_main_sound_spu_state.music_ramp.count = time;
            g_main_sound_spu_state.music_ramp.step = (difference / time) << 8;
        }
    }
}
