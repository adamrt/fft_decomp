/* LIBGPU 80024390-800244a4. */
#include "psx/libgpu.h"

int SetGraphReverse(int mode) {
    u8 old = g_psyq_gpu_environment.reversed;
    if (g_psyq_gpu_environment.debug_level >= 2)
        GPU_printf(g_psyq_gpu_graph_reverse_format, mode);
    g_psyq_gpu_environment.reversed = mode;
    g_psyq_gpu_dispatch->control(g_psyq_gpu_dispatch->cached_control(PSYQ_GPU_GP1_DISPLAY_MODE)
        | (g_psyq_gpu_environment.reversed ? PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_DISPLAY_MODE) | PSYQ_GPU_DISPLAY_REVERSE
                                           : PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_DISPLAY_MODE)));
    if (g_psyq_gpu_environment.graph_type == 2)
        g_psyq_gpu_dispatch->control(g_psyq_gpu_environment.reversed
                ? PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_VRAM_SIZE_V1) | 0x501
                : PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_VRAM_SIZE_V1) | 0x504);
    return old;
}
