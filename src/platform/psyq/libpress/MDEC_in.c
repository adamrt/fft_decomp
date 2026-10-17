#include "psx/libpress.h"

/* Send the MDEC command, then transfer size 32-bit parameters by DMA. */
void MDEC_in(u32* buf, u32 size) {
    MDEC_in_sync();
    *g_psyq_press_dma_dpcr |= PSYQ_DMA_MDEC_IN_ENABLE | PSYQ_DMA_MDEC_OUT_ENABLE;
    *g_psyq_press_dma_in_madr = (u32)(buf + 1);
    *g_psyq_press_dma_in_bcr = ((size / PSYQ_PRESS_DMA_BLOCK_WORDS) << 16) | PSYQ_PRESS_DMA_BLOCK_WORDS;
    *g_psyq_press_mdec_command = buf[0];
    *g_psyq_press_dma_in_chcr = PSYQ_DMA_BUSY | PSYQ_DMA_MODE_REQUEST | PSYQ_DMA_TO_DEVICE;
}
