/* LIBGPU 80024510-800245bc. */
#include "psx/libgpu.h"
int SetGraphQueue(int enabled) {
    u8 old = g_psyq_gpu_environment.queue_enabled;
    if (g_psyq_gpu_environment.debug_level >= 2)
        GPU_printf(g_psyq_gpu_graph_queue_format, enabled);
    if (enabled != g_psyq_gpu_environment.queue_enabled) {
        g_psyq_gpu_dispatch->reset(1);
        g_psyq_gpu_environment.queue_enabled = enabled;
        DMACallback(PSYQ_DMA_GPU_CHANNEL, 0);
    }
    return old;
}
