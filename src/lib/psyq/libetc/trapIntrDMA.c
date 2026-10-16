/* LIBETC 8001e614-8001e798; DMA completion dispatch and error diagnosis. */
#include "psx/libetc.h"

/* DMA IRQ handler: acknowledge each channel's completion flag and run its callback until none is
 * pending; on a bus error, or a master flag with no channel flag, print the DMA interrupt register
 * and every channel's address. */
void trapIntrDMA(void) {
    s32 channel;
    u32 pending = (*g_psyq_etc_dma_dicr >> PSYQ_DMA_IRQ_FLAG_SHIFT) & PSYQ_DMA_IRQ_CHANNEL_MASK;
    while (pending) {
        for (channel = 0; pending && channel < PSYQ_DMA_CHANNEL_COUNT; channel++, pending >>= 1) {
            if (pending & 1) {
                *g_psyq_etc_dma_dicr &= PSYQ_DMA_IRQ_CONTROL_MASK | (1 << (channel + PSYQ_DMA_IRQ_FLAG_SHIFT));
                if (g_psyq_etc_dma_callbacks[channel])
                    g_psyq_etc_dma_callbacks[channel]();
            }
        }
        pending = (*g_psyq_etc_dma_dicr >> PSYQ_DMA_IRQ_FLAG_SHIFT) & PSYQ_DMA_IRQ_CHANNEL_MASK;
    }
    if ((*g_psyq_etc_dma_dicr & PSYQ_DMA_IRQ_FLAGS_MASK) == PSYQ_DMA_IRQ_MASTER_FLAG
        || (*g_psyq_etc_dma_dicr & PSYQ_DMA_IRQ_BUS_ERROR)) {
        printf(g_psyq_etc_dma_error_format, *g_psyq_etc_dma_dicr);
        for (channel = 0; channel < PSYQ_DMA_CHANNEL_COUNT; channel++) {
            printf(g_psyq_etc_dma_channel_address_format, channel, g_psyq_etc_dma_channels[channel].address);
        }
    }
}
