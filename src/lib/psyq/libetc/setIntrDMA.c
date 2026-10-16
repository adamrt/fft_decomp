#include "psx/libetc.h"

/* Install or remove a DMA channel's completion callback and enable or disable that channel's
 * interrupt (keeping the master enable on). Returns the previous callback. */
psyq_interrupt_callback_t setIntrDMA(s32 channel, psyq_interrupt_callback_t callback) {
    psyq_interrupt_callback_t previous = g_psyq_etc_dma_callbacks[channel];
    if (callback != previous) {
        if (callback) {
            g_psyq_etc_dma_callbacks[channel] = callback;
            *g_psyq_etc_dma_dicr = (*g_psyq_etc_dma_dicr & PSYQ_DMA_IRQ_CONTROL_MASK) | PSYQ_DMA_IRQ_MASTER_ENABLE
                | (1 << (channel + PSYQ_DMA_IRQ_CHANNEL_ENABLE_SHIFT));
        } else {
            g_psyq_etc_dma_callbacks[channel] = 0;
            *g_psyq_etc_dma_dicr = ((*g_psyq_etc_dma_dicr & PSYQ_DMA_IRQ_CONTROL_MASK) | PSYQ_DMA_IRQ_MASTER_ENABLE)
                & ~(1 << (channel + PSYQ_DMA_IRQ_CHANNEL_ENABLE_SHIFT));
        }
    }
    return previous;
}
