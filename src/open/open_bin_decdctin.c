#include "fft/open.h"

/* DecDCTin-style entry: set the MDEC output-depth and mask bits of the
 * command word from mode, then start the input DMA. */
void open_bin_decdctin(s32* buf, s32 mode) {
    if (mode & 1) {
        *buf = *buf & 0xF7FFFFFF;
    } else {
        *buf = *buf | 0x08000000;
    }
    if (mode & 2) {
        *buf = *buf | 0x02000000;
    } else {
        *buf = *buf & 0xFDFFFFFF;
    }
    open_bin_mdec_in((u32*)buf, (u16)*buf);
}
