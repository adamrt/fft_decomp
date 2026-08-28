#include "fft/open.h"

/* Send an MDEC command word and start DMA of the following size words. */
void open_bin_mdec_in(u32* buf, u32 size) {
    open_bin_mdec_in_sync();
    *g_open_dma_dpcr |= 0x88;
    *g_open_dma_mdec_in_madr = (u32)(buf + 1);
    *g_open_dma_mdec_in_bcr = ((size >> 5) << 16) | 0x20;
    *g_open_mdec_command_reg = buf[0];
    *g_open_dma_mdec_in_chcr = 0x01000201;
}
