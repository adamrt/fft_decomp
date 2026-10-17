#include "psx/libcd.h"

/* LIBCD 0x8001e9fc-0x8001ea0c: address of the cached position record. */
CdlLOC* CdLastPos(void) {
    return &CD_pos;
}
