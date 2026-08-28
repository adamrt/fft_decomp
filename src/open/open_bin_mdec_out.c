#include "fft/open.h"

/* Start the MDEC output DMA of size words into buf. */
void open_bin_mdec_out(u32 buf, u32 size) {
    open_bin_mdec_out_sync();
    *g_open_dma_dpcr |= 0x88;
    *g_open_dma_mdec_out_chcr = 0;
    *g_open_dma_mdec_out_madr = buf;
    *g_open_dma_mdec_out_bcr = ((size >> 5) << 16) | 0x20;
    *g_open_dma_mdec_out_chcr = 0x01000200;
}
