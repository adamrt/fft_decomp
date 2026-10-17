/* LIBGPU 800258f0-800259bc. */
#include "psx/libgpu.h"

/* Build the GP0 drawing-area bottom-right command for (x, y), clamped to VRAM. */
u32 get_ce(s16 x, s16 y) {
    extern volatile psyq_gpu_environment_t
        g_psyq_gpu_environment; /* An unqualified RAM view removes the native clamp sign-extension steps. */
    limitRange(x, 0, g_psyq_gpu_environment.vram_width - 1);
    limitRange(y, 0, g_psyq_gpu_environment.vram_height - 1);
    if (g_psyq_gpu_environment.graph_type == 1 || g_psyq_gpu_environment.graph_type == 2)
        return PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_AREA_BOTTOM_RIGHT) | ((y & 0xfff) << 12) | (x & 0xfff);
    return PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_AREA_BOTTOM_RIGHT) | ((y & 0x3ff) << 10) | (x & 0x3ff);
}
