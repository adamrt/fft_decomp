/* LIBGPU 80024238-80024390. */
#include "psx/libetc.h"
#include "psx/libgpu.h"

int ResetGraph(int mode) {
    switch (mode & 7) {
    case 3:
    case 0:
        printf(g_psyq_gpu_reset_addresses_format, &g_psyq_gpu_dispatch_table, &g_psyq_gpu_environment);
        psyq_gpu_memset(&g_psyq_gpu_environment, 0, sizeof(g_psyq_gpu_environment));
        ResetCallback();
        GPU_cw((u32)g_psyq_gpu_dispatch & PSYQ_GPU_DMA_ADDRESS_MASK);
        g_psyq_gpu_environment.graph_type = _reset((mode & 7) != 0);
        g_psyq_gpu_environment.queue_enabled = 1;
        g_psyq_gpu_environment.vram_width = g_psyq_gpu_vram_width_lookup[g_psyq_gpu_environment.graph_type];
        g_psyq_gpu_environment.vram_height = g_psyq_gpu_vram_height_lookup[g_psyq_gpu_environment.graph_type];
        psyq_gpu_memset(&g_psyq_gpu_environment.draw, -1, sizeof(DRAWENV));
        psyq_gpu_memset(&g_psyq_gpu_environment.display, -1, sizeof(DISPENV));
        return g_psyq_gpu_environment.graph_type;
    }
    if (g_psyq_gpu_environment.debug_level >= 2)
        GPU_printf(g_psyq_gpu_reset_graph_format, mode);
    return g_psyq_gpu_dispatch->reset(1);
}
