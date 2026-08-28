#include "psx/types.h"

void equip_gfx_set_clut_rect_from_id(s16* rect, u32 clut_id) {
    rect[0] = (clut_id & 0x3F) << 4;
    rect[1] = (clut_id & 0xFFFF) >> 6;
    rect[2] = 0x10;
    rect[3] = 1;
}
