/* SCUS_942.21 0x8002004c..0x800200d3. */
#include "psx/libcd.h"

/* Write a CdlATV mix to the CD controller's audio attenuation registers and apply it; always
 * returns 0. */
s32 CD_vol(CdlATV* volume) {
    psyq_cd_apply_volume(volume);
    return 0;
}
