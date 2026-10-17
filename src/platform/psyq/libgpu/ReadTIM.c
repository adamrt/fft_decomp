/* LIBGPU 80026fb4-8002701c. */
#include "psx/libgpu.h"

TIM_IMAGE* ReadTIM(TIM_IMAGE* image) {
    int size = psyq_gpu_decode_tim(g_psyq_gpu_tim_cursor, image);
    if (size == -1)
        return 0;
    g_psyq_gpu_tim_cursor += size;
    return image;
}
