#include "psx/libcd.h"

/* LIBCD 0x8001e9cc-0x8001e9dc: cached controller status byte. */
int CdStatus(void) {
    u8 status = CD_status;
    return status;
}
