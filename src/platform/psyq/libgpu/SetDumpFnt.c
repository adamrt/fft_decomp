/* LIBGPU 80022eec-80022f2c. */
#include "psx/libgpu.h"
void SetDumpFnt(int window) {
    if (window < 0 || g_psyq_gpu_font_window_count < window)
        return;
    g_psyq_gpu_default_font_window = window;
    GPU_printf = (int (*)(const char*, ...))FntPrint;
}
