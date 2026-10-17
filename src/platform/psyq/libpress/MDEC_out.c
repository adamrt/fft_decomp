#include "psx/libpress.h"

/* Start the MDEC output DMA of size 32-bit values into buf. */
void MDEC_out(u32* buf, u32 size) {
    MDEC_out_sync();
    *g_psyq_press_dma_dpcr |= PSYQ_DMA_MDEC_IN_ENABLE | PSYQ_DMA_MDEC_OUT_ENABLE;
    *g_psyq_press_dma_out_chcr = 0;
    *g_psyq_press_dma_out_madr = (u32)buf;
    *g_psyq_press_dma_out_bcr = ((size / PSYQ_PRESS_DMA_BLOCK_WORDS) << 16) | PSYQ_PRESS_DMA_BLOCK_WORDS;
    *g_psyq_press_dma_out_chcr = PSYQ_DMA_BUSY | PSYQ_DMA_MODE_REQUEST;
}
