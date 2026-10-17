/* SCUS_942.21 0x8001ea0c..0x8001ea77. */
#include "psx/libcd.h"

s32 CdReset(s32 mode) {
    if (mode == 2) {
        CD_initintr();
        return 1;
    }
    if (CD_init())
        return 0;
    if (mode == 1 && CD_initvol())
        return 0;
    return 1;
}
