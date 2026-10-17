/* LIBGPU 800244a4-80024510. */
#include "psx/libgpu.h"

int SetGraphDebug(int level) {
    u8 old = g_psyq_gpu_environment.debug_level;
    u8 debug_level = level;
    g_psyq_gpu_environment.debug_level = debug_level;
    if (debug_level) {
        GPU_printf(g_psyq_gpu_graph_debug_format, g_psyq_gpu_environment.debug_level, g_psyq_gpu_environment.graph_type,
            g_psyq_gpu_environment.reversed);
    }
    return old;
}
