/* LIBGPU 800245dc-80024638. */
#include "psx/libetc.h"
#include "psx/libgpu.h"

psyq_interrupt_callback_t DrawSyncCallback(psyq_interrupt_callback_t callback) {
    psyq_interrupt_callback_t old;
    if (g_psyq_gpu_environment.debug_level >= 2) {
        GPU_printf(g_psyq_gpu_draw_sync_callback_format, callback);
    }
    old = g_psyq_gpu_environment.draw_sync_callback;
    g_psyq_gpu_environment.draw_sync_callback = callback;
    return old;
}
