/* LIBGPU 80022d10-80022d78. */
#include "psx/libgpu.h"

u16 LoadClut(u32* pixels, s32 x, s32 y) {
    RECT rect;
    setRECT(&rect, x, y, 256, 1);
    LoadImage(&rect, pixels);
    return GetClut(x, y);
}
