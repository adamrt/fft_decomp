/* LIBGPU 800259bc-80025a04. */
#include "psx/libgpu.h"

/* Build the GP0 draw-offset command for (x, y). */
u32 get_ofs(s16 x, s16 y) {
    /* The volatile view preserves separate graph-type address construction. */
    volatile psyq_gpu_environment_t* environment = &g_psyq_gpu_environment;
    u8 graph_type = environment->graph_type;
    if (graph_type == 1 || graph_type == 2)
        return PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_OFFSET) | ((y & 0xfff) << 12) | (x & 0xfff);
    return PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_OFFSET) | ((y & 0x7ff) << 11) | (x & 0x7ff);
}
