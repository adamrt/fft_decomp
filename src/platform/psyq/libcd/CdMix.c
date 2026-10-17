#include "psx/libcd.h"

s32 CdMix(CdlATV* volume) {
    CD_vol(volume);
    return 1;
}
