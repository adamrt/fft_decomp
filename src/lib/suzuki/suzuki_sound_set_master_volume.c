#include "fft/suzuki.h"
#include "psx/libspu.h"
#include "psx/types.h"

/* Sets the master volume, at once or as a ramp of time ticks.
 *
 * The root-counter handler steps music_ramp and applies it to common.mvol;
 * a ramp is only started when the 8.8 volume actually changes. */
void suzuki_sound_set_master_volume(s16 volume, s16 time) {
    s32 difference;

    g_suzuki_sound_spu_state.music_ramp.target = volume;
    if (time == 0) {
        g_suzuki_sound_spu_state.music_ramp.value = volume << 16;
        g_suzuki_sound_spu_state.music_ramp.count = 0;
        g_suzuki_sound_spu_state.music_volume = volume;
        suzuki_sound_set_vol_balance(volume, &g_suzuki_sound_spu_state.common.mvol, 0);
        g_suzuki_sound_spu_state.common.mask |= SPU_COMMON_MVOLL | SPU_COMMON_MVOLR;
    } else {
        difference = ((volume << 16) >> 8) - (g_suzuki_sound_spu_state.music_ramp.value >> 8);
        if (difference != 0) {
            g_suzuki_sound_spu_state.music_ramp.count = time;
            g_suzuki_sound_spu_state.music_ramp.step = (difference / time) << 8;
        }
    }
}
