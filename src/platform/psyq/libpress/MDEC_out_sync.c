#include "psx/libpress.h"
#include "psx/types.h"

/* Spin until the MDEC output DMA finishes, reporting a timeout through the
 * library's diagnostic dump when the counter expires. */
s32 MDEC_out_sync(void) {
    volatile s32 count = PSYQ_PRESS_SYNC_POLL_LIMIT;

    while (*g_psyq_press_dma_out_chcr & PSYQ_DMA_BUSY) {
        if (--count == -1) {
            timeout(g_psyq_press_out_sync_name);
            return -1;
        }
    }
    return 0;
}
