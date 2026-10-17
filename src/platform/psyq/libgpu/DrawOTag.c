/* LIBGPU 80024c38-80024cac. */
#include "psx/libgpu.h"

void DrawOTag(u32* ot) {
    if (g_psyq_gpu_environment.debug_level >= 2) {
        GPU_printf(g_psyq_gpu_draw_otag_format, ot);
    }
    g_psyq_gpu_dispatch->enqueue_four(g_psyq_gpu_dispatch->ordering_table, ot, 0, 0);
}
