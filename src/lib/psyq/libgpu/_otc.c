/* LIBGPU 80025b5c-80025c44. */
#include "psx/libgpu.h"
/* Clear count ordering-table entries at ot with the OTC DMA channel, which links each entry to the
 * one below it starting from the last; waits for it. Returns count, or -1 on timeout. */
int _otc(u32* ot, int count) {
    u32* position = ot;
    int final_offset;
    *g_psyq_gpu_dma_dpcr |= PSYQ_DMA_OTC_ENABLE;
    *g_psyq_gpu_otc_dma_chcr = 0;
    /* Typed entry arithmetic moves the final subtraction after the base addition. */
    final_offset = count * 4 - 4;
    position = (u32*)((u8*)position + final_offset);
    *g_psyq_gpu_otc_dma_madr = (u32)position;
    *g_psyq_gpu_otc_dma_bcr = count;
    *g_psyq_gpu_otc_dma_chcr = PSYQ_DMA_BUSY | PSYQ_DMA_TRIGGER | PSYQ_DMA_ADDRESS_DECREMENT;
    set_alarm();
    while (*g_psyq_gpu_otc_dma_chcr & PSYQ_DMA_BUSY) {
        if (get_alarm())
            return -1;
    }
    return count;
}
