/* LIBGPU 800257c8-80025824. */
#include "psx/libgpu.h"

/* Build the GP0 draw-mode command from a texture page and the draw-to-display and dither flags, in
 * the bit layout of the detected GPU type. */
u32 get_mode(int dfe, int dtd, u16 tpage) {
    /* The volatile view preserves separate graph-type address construction. */
    volatile psyq_gpu_environment_t* environment = &g_psyq_gpu_environment;
    u8 graph_type;
    graph_type = environment->graph_type;
    if (graph_type == 1 || graph_type == 2)
        return PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_MODE) | (dtd ? 0x800 : 0) | (dfe ? 0x1000 : 0)
            | (tpage & 0x27ff);
    return PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_MODE) | (dtd ? 0x200 : 0) | (dfe ? 0x400 : 0) | (tpage & 0x9ff);
}
