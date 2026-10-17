/* LIBGPU 80024bd8-80024c38. */
#include "psx/libgpu.h"

void DrawPrim(void* primitive) {
    int count = getlen(primitive);
    g_psyq_gpu_dispatch->sync(0);
    g_psyq_gpu_dispatch->commands((u32*)primitive + 1, count);
}
