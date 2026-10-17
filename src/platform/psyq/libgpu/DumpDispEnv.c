/* LIBGPU 8002418c-80024238. */
#include "psx/libgpu.h"
void DumpDispEnv(DISPENV* env) {
    GPU_printf(g_psyq_gpu_display_rectangle_format, env->disp.x, env->disp.y, env->disp.w, env->disp.h);
    GPU_printf(g_psyq_gpu_screen_rectangle_format, env->screen.x, env->screen.y, env->screen.w, env->screen.h);
    GPU_printf(g_psyq_gpu_interlace_format, env->isinter);
    GPU_printf(g_psyq_gpu_rgb24_format, env->isrgb24);
}
