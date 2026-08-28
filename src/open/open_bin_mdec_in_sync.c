#include "fft/open.h"
#include "psx/types.h"

/* OPEN.BIN MDEC in sync - spin until the MDEC input FIFO drains, reporting a
 * timeout through the library's diagnostic dump when the counter expires. */
s32 open_bin_mdec_in_sync(void) {
    volatile s32 timeout = 0x100000;

    while (*g_open_mdec_status_reg & 0x20000000) {
        if (--timeout == -1) {
            open_bin_libpress_timeout(g_open_text_mdec_in_sync_tag);
            return -1;
        }
    }
    return 0;
}
