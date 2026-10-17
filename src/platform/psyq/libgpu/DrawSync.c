/* LIBGPU 800246d4-80024740. */
#include "psx/libgpu.h"

int DrawSync(int mode) {
    if (g_psyq_gpu_environment.debug_level >= 2) {
        GPU_printf(g_psyq_gpu_draw_sync_format, mode);
    }
    return g_psyq_gpu_dispatch->sync(mode);
}
