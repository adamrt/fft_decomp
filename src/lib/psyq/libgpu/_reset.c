/* LIBGPU 80026a58-80026b94. */
#include "psx/libgpu.h"
/* Empty the GPU command queue with interrupts masked. Mode 0 also resets the GPU, clears the GP1
 * cache and the queue, and returns the detected GPU type; mode 1 only acknowledges the GPU IRQ and
 * clears its command buffer. */
int _reset(int mode) {
    int mask = SetIntrMask(0);
    int tail;
    _qout = 0; /* Queue indices are observed by DMA interrupt callbacks. */
    tail = _qout;
    g_psyq_gpu_reset_interrupt_mask = mask;
    _qin = tail;
    switch (mode & 7) {
    case 0:
        *g_psyq_gpu_dma_chcr = PSYQ_DMA_MODE_LINKED_LIST | PSYQ_DMA_TO_DEVICE;
        *g_psyq_gpu_dma_dpcr |= PSYQ_DMA_GPU_ENABLE;
        *g_psyq_gpu_gp1_port = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_RESET);
        psyq_gpu_memset(g_psyq_gpu_gp1_cache, 0, sizeof(g_psyq_gpu_gp1_cache));
        psyq_gpu_memset(_que, 0, sizeof(_que));
        break;
    case 1:
        *g_psyq_gpu_dma_chcr = PSYQ_DMA_MODE_LINKED_LIST | PSYQ_DMA_TO_DEVICE;
        *g_psyq_gpu_dma_dpcr |= PSYQ_DMA_GPU_ENABLE;
        *g_psyq_gpu_gp1_port = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_ACK_IRQ);
        *g_psyq_gpu_gp1_port = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_RESET_BUFFER);
        break;
    }
    SetIntrMask(g_psyq_gpu_reset_interrupt_mask);
    if ((mode & 7) == 0)
        return psyq_gpu_detect_type(mode);
    return 0;
}
