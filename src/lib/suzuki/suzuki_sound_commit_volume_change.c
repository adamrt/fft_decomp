#include "fft/suzuki.h"
#include "psx/libspu.h"

/* Rebuilds the master, CD and reverb-depth volume pairs for the current
 * sound type and marks them for the next SpuSetCommonAttr and
 * SpuSetReverbDepth. */
void suzuki_sound_commit_volume_change(void) {
    suzuki_sound_set_vol_balance(g_suzuki_sound_spu_state.music_volume, &g_suzuki_sound_spu_state.common.mvol, 0);
    suzuki_sound_set_vol_balance(g_suzuki_sound_spu_state.cd_volume, &g_suzuki_sound_spu_state.common.cd.volume, 0);
    suzuki_sound_set_vol_balance(g_suzuki_sound_spu_state.reverb_depth, &g_suzuki_sound_reverb_attr.depth, 1);
    g_suzuki_sound_spu_state.common.mask |= SPU_COMMON_MVOLL | SPU_COMMON_MVOLR | SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
    g_suzuki_sound_reverb_attr.mask |= SPU_REV_DEPTHL | SPU_REV_DEPTHR;
}
