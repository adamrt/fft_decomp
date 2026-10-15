#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Clear the whole 256x480 frame buffer, then install the draw-clip rectangle
 * for the requested world-map layout: mode 0 leaves an 8-pixel top margin and
 * a 252x228 area, any other mode uses the full 256x238 screen. */
void wldcore_clear_screen_and_set_map_clip_rect(s32 mode) {
    RECT rect;

    rect.x = 0;
    rect.y = 0;
    rect.w = 256;
    rect.h = 480;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_SCREEN_CLEARED;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    if (mode == 0) {
        rect.x = 0;
        rect.y = 8;
        rect.w = 252;
        rect.h = 228;
    } else {
        rect.x = 0;
        rect.y = 0;
        rect.w = 256;
        rect.h = 238;
    }
    GsSetClip2D(&rect);
    GsSetDrawBuffClip();
}
