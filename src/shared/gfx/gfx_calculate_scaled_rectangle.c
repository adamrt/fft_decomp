#include "fft/gfx.h"

/* Build the centerd sub-rectangle of the source area for a scale mode
 * (20%, 50%, 80%, 90% or 100%), aligned to 4 pixels. */
void gfx_calculate_scaled_rectangle(s32 scale_mode, const RECT* source, RECT* rectangle) {
    s32 scale;
    s32 source_width;
    s32 source_height;
    s32 half_width;
    s32 half_height;
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

    source_width = source->w;
    source_height = source->h;
    half_width = (source_width / 2) & 0xfffc;
    half_height = (source_height / 2) & 0xfffc;
    width = (source_width * scale / 200) & 0xfffc;
    height = (source_height * scale / 200) & 0xfffc;
    if (half_width < width) {
        width = half_width;
    }
    if (half_height < height) {
        height = half_height;
    }
    rectangle->x = half_width - width;
    rectangle->y = half_height - height;
    rectangle->w = width * 2;
    rectangle->h = height * 2;
}
