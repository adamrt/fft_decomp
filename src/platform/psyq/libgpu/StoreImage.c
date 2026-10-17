/* LIBGPU 80024960-800249c4. */
#include "psx/libgpu.h"

s32 StoreImage(RECT* rect, u32* pixels) {
    checkRECT(g_psyq_gpu_store_image_name, rect);
    return g_psyq_gpu_dispatch->enqueue_four(g_psyq_gpu_dispatch->store, rect, sizeof(*rect), (u32)pixels);
}
