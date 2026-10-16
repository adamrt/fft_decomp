/* SCUS_942.21 0x800192ec..0x8001934b. */
#include "psx/libspu.h"

/* Set the SPU DMA channel's priority in the DMA control register: 3 when enabled is nonzero, else
 * 5; the channel's enable bit is left alone. Nothing in the tree calls it. */
void _spu_FsetPCR(s32 enabled) {
    *g_psyq_spu_dma_priority_register &= 0xfff8ffff;
    if (enabled) {
        *g_psyq_spu_dma_priority_register |= 0x30000;
    } else {
        *g_psyq_spu_dma_priority_register |= 0x50000;
    }
}
