/* LIBGPU 80026fa4-80026fb4. */
#include "psx/libgpu.h"

s32 OpenTIM(u32* tim) {
    g_psyq_gpu_tim_cursor = tim;
    return 0;
}
