#include "psx/gpu.h"

void world_gfx_set_clut_rect_from_id(RECT* rect, s32 clut_id) {
    s16 x;

    x = (clut_id & 0x3f) << 4;
    clut_id &= 0xffff;
    clut_id = (u32)clut_id >> 6;
    rect->x = x;
    rect->w = 0x10;
    rect->y = clut_id;
    rect->h = 1;
}
