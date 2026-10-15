#include "fft/wldcore.h"

/* Fill the rectangle with the 256x238 display area of frame
 * buffer 0 (y 0) or 1 (y 0xF0). */
void wldcore_gfx_set_display_rect(s32 buffer_index, RECT* rect) {
    rect->w = 0x100;
    rect->x = 0;
    rect->h = 0xEE;
    if (buffer_index == 0) {
        rect->y = 0;
    } else {
        rect->y = 0xF0;
    }
}
