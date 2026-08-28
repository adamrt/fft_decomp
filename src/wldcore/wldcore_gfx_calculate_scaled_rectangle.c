#include "fft/wldcore.h"
#include "psx/types.h"

/* Build the centred sub-rectangle of source's w x h area for a scale mode
 * (20%, 50%, 80%, 90% or 100%), aligned to 4 pixels. */
void wldcore_gfx_calculate_scaled_rectangle(
    s32 scale_mode, const wldcore_display_rect_t* source, wldcore_display_rect_t* rectangle) {
    s32 scale;
    s32 x;
    s32 y;
    s32 scaled_x;
    s32 scaled_y;
    s32 half_x;
    s32 half_y;
    s32 width;
    s32 height;

    switch (scale_mode) {
    case 0:
        scale = 20;
        break;
    case 1:
        scale = 50;
        break;
    case 2:
        scale = 80;
        break;
    case 3:
        scale = 90;
        break;
    default:
        scale = 100;
        break;
    }

    x = (s16)source->w;
    y = (s16)source->h;
    scaled_x = x * scale;
    scaled_y = y * scale;
    half_x = (x / 2) & 0xfffc;
    half_y = (y / 2) & 0xfffc;
    width = (scaled_x / 200) & 0xfffc;
    height = (scaled_y / 200) & 0xfffc;
    if (half_x < width) {
        width = half_x;
    }
    if (half_y < height) {
        height = half_y;
    }
    rectangle->u = half_x - width;
    rectangle->v = half_y - height;
    rectangle->w = width * 2;
    rectangle->h = height * 2;
}
