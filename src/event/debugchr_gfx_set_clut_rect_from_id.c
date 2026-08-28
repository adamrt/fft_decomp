#include "psx/gpu.h"

void debugchr_gfx_set_clut_rect_from_id(RECT* rect, s32 packed) {
    s16 x;

    x = (packed & 0x3f) << 4;
    packed &= 0xffff;
    packed = (u32)packed >> 6;
    rect->x = x;
    rect->w = 0x10;
    rect->y = packed;
    rect->h = 1;
}
