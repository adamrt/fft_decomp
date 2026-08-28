#include "psx/etc.h"

/* OPEN.BIN open_bin_decdctreset - wrapper around the DCT reset routine that also
 * clears the BIOS callback table when initializing from scratch. */
void open_bin_decdctreset(s32 mode) {
    if (mode == 0) {
        ResetCallback();
    }
    open_mdec_reset(mode);
}
