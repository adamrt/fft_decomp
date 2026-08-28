#include "fft/main_sound.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Rebuilds the master, CD and reverb-depth volume pairs for the current
 * sound type and marks them for the next SpuSetCommonAttr and
 * SpuSetReverbDepth. */
void main_sound_commit_volume_change(void) {
    main_sound_set_vol_balance(g_main_sound_spu_state.music_volume, &g_main_sound_spu_state.common.mvol, 0);
    main_sound_set_vol_balance(g_main_sound_spu_state.cd_volume, &g_main_sound_spu_state.common.cd.volume, 0);
    main_sound_set_vol_balance(g_main_sound_spu_state.reverb_depth, &g_main_sound_reverb_attr.depth, 1);
    g_main_sound_spu_state.common.mask |= 0xc3;
    g_main_sound_reverb_attr.mask |= 6;
}
