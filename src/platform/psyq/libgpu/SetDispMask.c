/* LIBGPU 80024638-800246d4. */
#include "psx/libgpu.h"
void SetDispMask(int mask) {
    u32 command;
    psyq_gpu_dispatch_t* dispatch;
    if (g_psyq_gpu_environment.debug_level >= 2)
        GPU_printf(g_psyq_gpu_display_mask_format, mask);
    if (!mask)
        psyq_gpu_memset(&g_psyq_gpu_environment.display, -1, sizeof(DISPENV));
    dispatch = g_psyq_gpu_dispatch;
    command = mask ? PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_DISPLAY_ENABLE)
                   : (PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_GP1_DISPLAY_ENABLE) | PSYQ_GPU_DISPLAY_DISABLED);
    dispatch->control(command);
}
