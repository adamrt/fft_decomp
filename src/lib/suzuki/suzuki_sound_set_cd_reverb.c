#include "fft/suzuki.h"
#include "psx/libspu.h"
#include "psx/types.h"

/* Sets the CD input's reverb and mix switches and applies the common
 * attributes. */
void suzuki_sound_set_cd_reverb(s32 reverb, s32 mix) {
    g_suzuki_sound_spu_state.common.cd.reverb = reverb;
    g_suzuki_sound_spu_state.common.cd.mix = mix;
    g_suzuki_sound_spu_state.common.mask |= SPU_COMMON_CDREV | SPU_COMMON_CDMIX;
    SpuSetCommonAttr(&g_suzuki_sound_spu_state.common);
}
