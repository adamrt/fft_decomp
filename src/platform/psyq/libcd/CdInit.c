/* SCUS_942.21 0x8001e8c4..0x8001e953. */
#include "psx/libcd.h"
s32 CdInit(void) {
    s32 retry = 4;
    s32 initialized;
retry_reset:
    if (CdReset(1) == 1) {
        CdSyncCallback(def_cbsync);
        CdReadyCallback(def_cbready);
        CdReadCallback(def_cbread);
        initialized = 1;
        goto init_done;
    }
    if (--retry != -1)
        goto retry_reset;
    printf(g_psyq_cd_init_failure_message);
    initialized = 0;
init_done:
    return initialized;
}
