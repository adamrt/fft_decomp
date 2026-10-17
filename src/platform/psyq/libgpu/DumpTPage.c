/* LIBGPU 80023a6c-80023b3c. */
#include "psx/libgpu.h"
void DumpTPage(u16 tpage) {
    if (GetGraphType() == 1 || GetGraphType() == 2) {
        GPU_printf(
            g_psyq_gpu_tpage_format, (tpage >> 9) & 3, (tpage >> 7) & 3, (tpage << 6) & 0x7c0, (tpage << 3) & 0x300);
    } else {
        GPU_printf(g_psyq_gpu_tpage_format, (tpage >> 7) & 3, (tpage >> 5) & 3, (tpage << 6) & 0x7c0,
            ((tpage << 4) & 0x100) + ((tpage >> 2) & 0x200));
    }
}
