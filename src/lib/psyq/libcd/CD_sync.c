/* SCUS_942.21 0x8001f6b8..0x8001f937. */
#include "psx/libcd.h"
#include "psx/libetc.h"

/* Wait (mode 0) or poll once (mode nonzero) for the current command to finish: returns CdlComplete
 * or CdlDiskError with the response copied to result, 0 when polling finds it pending, -1 on
 * timeout. Inside an interrupt callback it services the CD interrupt itself. */
s32 CD_sync(s32 mode, u8* result) {
    s32 event;
    psyq_cd_set_timeout(g_psyq_cd_sync_timeout_name);
    while (1) {
        if (psyq_cd_check_timeout())
            return -1;
        if (CheckCallback())
            psyq_cd_dispatch_callbacks();
        event = g_psyq_cd_irq_state.sync;
        if (event == CdlComplete || event == CdlDiskError) {
            g_psyq_cd_irq_state.sync = CdlComplete;
            psyq_cd_copy_result(result, g_psyq_cd_sync_result, 8);
            return event;
        }
        if (mode)
            return 0;
    }
}
