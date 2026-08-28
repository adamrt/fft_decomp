#include "fft/open.h"
#include "psx/types.h"

/* OPEN.BIN MDEC out sync - spin until the MDEC output FIFO empties, reporting
 * a timeout through the library's diagnostic dump when the counter expires. */
s32 open_bin_mdec_out_sync(void) {
    volatile s32 timeout = 0x100000;

    while (*g_open_dma_mdec_out_chcr & 0x01000000) {
        if (--timeout == -1) {
            open_bin_libpress_timeout(g_open_text_mdec_out_sync_tag);
            return -1;
        }
    }
    return 0;
}
