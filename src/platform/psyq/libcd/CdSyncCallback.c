#include "psx/libcd.h"

CdlCB CdSyncCallback(CdlCB callback) {
    CdlCB previous = CD_cbsync;
    CD_cbsync = callback;
    return previous;
}
