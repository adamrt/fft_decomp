/* LIBGPU 8002634c-80026374. */
#include "psx/libgpu.h"

/* Write a GP1 control command and remember it by command number for _getctl. */
void _ctl(u32 command) {
    *g_psyq_gpu_gp1_port = command;
    g_psyq_gpu_gp1_cache[command >> 24] = command;
}
