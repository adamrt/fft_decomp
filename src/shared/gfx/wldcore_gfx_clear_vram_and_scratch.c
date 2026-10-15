#include "fft/wldcore.h"
#include "psx/libgpu.h"

/* Clear both frame buffers (256 x 480 at the VRAM origin) to black and, when
 * asked, zero the 0x7800-entry scratch buffer. */
void wldcore_gfx_clear_vram_and_scratch(s32 clear_scratch) {
    RECT image_rect;
    s32 i;

    DrawSync(0);
    VSync(0);
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 0x100;
    image_rect.h = 0x1e0;
    ClearImage(&image_rect, 0, 0, 0);
    if (clear_scratch != 0) {
        for (i = 0x77ff; i >= 0; i--) {
            g_wldcore_scratch_buffer[i] = 0;
        }
    }
    DrawSync(0);
}
