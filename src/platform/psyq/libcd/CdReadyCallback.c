#include "psx/libcd.h"

CdlCB CdReadyCallback(CdlCB callback) {
    CdlCB previous = CD_cbready;
    CD_cbready = callback;
    return previous;
}
