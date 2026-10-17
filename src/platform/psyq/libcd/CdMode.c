#include "psx/libcd.h"

/* LIBCD 0x8001e9dc-0x8001e9ec: cached CD mode byte. */
s32 CdMode(void) {
    return CD_mode;
}
