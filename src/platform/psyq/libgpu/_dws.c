#include "psx/libgpu.h"

/* LIBGPU, retail 0x80025e5c-0x800260b0. VRAM transfer in 16-bit units, with a CPU remainder and 64-byte DMA blocks. */
int _dws(RECT* rect, u32* data) {
    extern volatile psyq_gpu_environment_t g_psyq_gpu_environment; /* An unqualified RAM view removes native 16-bit
                                                                      normalization in the bounds clamps. */
    int size;
    int blocks;
    int special; /* The zero selector retains a native branch and an extra saved register. */
    int rest;
    set_alarm();
    special = 0;
    limitRange(rect->w, 0, g_psyq_gpu_environment.vram_width);
    limitRange(rect->h, 0, g_psyq_gpu_environment.vram_height);
    size = (rect->w * rect->h + 1) / 2;
    /* This overwritten value keeps the native register copy before the remainder calculation. */
    rest = size >> 4;
    if (size <= 0)
        return -1;
    rest = size - ((size >> 4) << 4);
    blocks = size >> 4;
    while (!(*g_psyq_gpu_gp1_port & PSYQ_GPU_STATUS_READY_COMMAND)) {
        if (get_alarm())
            return -1;
    }
    *g_psyq_gpu_gp1_port = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_DMA_DIRECTION);
    *g_psyq_gpu_gp0_port = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_CLEAR_CACHE);
    *g_psyq_gpu_gp0_port = special ? 0xb0000000 : PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_CPU_TO_VRAM);
    /* Component packing replaces the retail 32-bit GPU loads with 16-bit loads. */
    *g_psyq_gpu_gp0_port = *(u32*)&rect->x;
    *g_psyq_gpu_gp0_port = *(u32*)&rect->w;
    while (rest--) {
        *g_psyq_gpu_gp0_port = *data++;
    }
    if (blocks) {
        *g_psyq_gpu_gp1_port = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_DMA_DIRECTION) | PSYQ_GPU_DMA_REQUEST_WRITE;
        *g_psyq_gpu_dma_madr = (u32)data;
        *g_psyq_gpu_dma_bcr = (blocks << 16) | PSYQ_GPU_DMA_IMAGE_BLOCK_WORDS;
        *g_psyq_gpu_dma_chcr = PSYQ_DMA_BUSY | PSYQ_DMA_MODE_REQUEST | PSYQ_DMA_TO_DEVICE;
    }
    return 0;
}
