#include "psx/libcd.h"

/* LIBCD 0x8001e9ec-0x8001e9fc: last CD command byte. */
s32 CdLastCom(void) {
    return CD_com;
}
