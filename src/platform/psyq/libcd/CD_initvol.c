/* SCUS_942.21 0x800201b4..0x800202a7. */
#include "psx/libcd.h"

/* Set the SPU's CD input volume to full (and the main volume too when it is zero), enable the SPU
 * with CD audio on, and route CD left to left and right to right at 0x80. */
s32 CD_initvol(void) {
    CdlATV volume;
    if (g_psyq_cd_spu_registers->current_main_volume_left == 0
        && g_psyq_cd_spu_registers->current_main_volume_right == 0) {
        g_psyq_cd_spu_registers->main_volume_left = 0x3fff;
        g_psyq_cd_spu_registers->main_volume_right = 0x3fff;
    }
    g_psyq_cd_spu_registers->cd_volume_left = 0x3fff;
    g_psyq_cd_spu_registers->cd_volume_right = 0x3fff;
    g_psyq_cd_spu_registers->control = 0xc001;
    volume.val0 = volume.val2 = 0x80;
    volume.val1 = volume.val3 = 0;
    psyq_cd_apply_volume(&volume);
    return 0;
}
