/* LIBGPU 80026374-80026388. */
#include "psx/libgpu.h"

/* Return the last GP1 command written for command number. */
u32 _getctl(int command) {
    return g_psyq_gpu_gp1_cache[command];
}
