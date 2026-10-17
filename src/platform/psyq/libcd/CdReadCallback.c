#include "psx/libcd.h"

CdlCB CdReadCallback(CdlCB callback) {
    CdlCB previous = g_psyq_cd_read_callback;
    g_psyq_cd_read_callback = callback;
    return previous;
}
