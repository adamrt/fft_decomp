#include "fft/main.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Sets the CD audio volume, at once or as a ramp of time ticks; the
 * CD twin of main_sound_set_master_volume, applied to common.cd.volume. */
void main_sound_set_cd_volume(s16 volume, s16 time) {
    s32 difference;

    g_main_sound_spu_state.cd_ramp.target = volume;
    if (time == 0) {
        g_main_sound_spu_state.cd_ramp.value = volume << 16;
        g_main_sound_spu_state.cd_ramp.count = 0;
        g_main_sound_spu_state.cd_volume = volume;
        main_sound_set_vol_balance(volume, &g_main_sound_spu_state.common.cd.volume, 0);
        g_main_sound_spu_state.common.mask |= 0xc0;
    } else {
        difference = ((volume << 16) >> 8) - (g_main_sound_spu_state.cd_ramp.value >> 8);
        if (difference != 0) {
            g_main_sound_spu_state.cd_ramp.count = time;
            g_main_sound_spu_state.cd_ramp.step = (difference / time) << 8;
        }
    }
}
