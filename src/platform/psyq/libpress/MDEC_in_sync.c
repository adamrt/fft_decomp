#include "psx/libpress.h"
#include "psx/types.h"

/* Wait until the MDEC command finishes; dump decoder state if polling expires. */
s32 MDEC_in_sync(void) {
    volatile s32 count = PSYQ_PRESS_SYNC_POLL_LIMIT;

    while (*g_psyq_press_mdec_status & PSYQ_MDEC_STATUS_BUSY) {
        if (--count == -1) {
            timeout(g_psyq_press_in_sync_name);
            return -1;
        }
    }
    return 0;
}
