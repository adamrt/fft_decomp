/* LIBGPU 80024740-80024868: debug-only rectangle diagnosis. */
#include "psx/libgpu.h"

/* Debug check of a VRAM rectangle: at graph debug level 1 report one that is empty or leaves VRAM,
 * at level 2 trace every call with its rectangle. */
void checkRECT(const char* name, RECT* rect) {
    switch (g_psyq_gpu_environment.debug_level) {
    case 1:
        if (rect->w > g_psyq_gpu_environment.vram_width || rect->w + rect->x > g_psyq_gpu_environment.vram_width
            || rect->y > g_psyq_gpu_environment.vram_height || rect->y + rect->h > g_psyq_gpu_environment.vram_height
            || rect->w <= 0 || rect->x < 0 || rect->y < 0 || rect->h <= 0) {
            GPU_printf(g_psyq_gpu_bad_rectangle_format, name);
            GPU_printf(g_psyq_gpu_rectangle_format, rect->x, rect->y, rect->w, rect->h);
        }
        break;
    case 2:
        GPU_printf(g_psyq_gpu_rectangle_operation_format, name);
        GPU_printf(g_psyq_gpu_rectangle_format, rect->x, rect->y, rect->w, rect->h);
        break;
    }
}
