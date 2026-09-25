#include "fft/main.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Sets the CD input's reverb and mix switches and applies the common
 * attributes. */
void main_sound_set_cd_reverb(s32 reverb, s32 mix) {
    g_main_sound_spu_state.common.cd.reverb = reverb;
    g_main_sound_spu_state.common.cd.mix = mix;
    g_main_sound_spu_state.common.mask |= 0x300;
    SpuSetCommonAttr(&g_main_sound_spu_state.common);
}
