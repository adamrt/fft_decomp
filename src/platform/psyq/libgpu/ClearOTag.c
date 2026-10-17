/* LIBGPU 80024a88-80024b40. */
#include "psx/libgpu.h"
u32* ClearOTag(u32* ot, int count) {
    if (g_psyq_gpu_environment.debug_level >= 2)
        GPU_printf(g_psyq_gpu_clear_otag_format, ot, count);
    while (--count) {
        setlen(ot, 0);
        setaddr(ot, ot + 1);
        ot++;
    }
    *ot = (u32)&g_psyq_gpu_ot_terminator & PSYQ_GPU_DMA_ADDRESS_MASK;
    return ot;
}
