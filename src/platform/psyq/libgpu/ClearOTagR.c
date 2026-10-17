/* LIBGPU 80024b40-80024bd8. */
#include "psx/libgpu.h"
u32* ClearOTagR(u32* ot, int count) {
    if (g_psyq_gpu_environment.debug_level >= 2)
        GPU_printf(g_psyq_gpu_clear_otag_reverse_format, ot, count);
    g_psyq_gpu_dispatch->clear_ot_reverse(ot, count);
    *ot = (u32)&g_psyq_gpu_ot_terminator & PSYQ_GPU_DMA_ADDRESS_MASK;
    return ot;
}
