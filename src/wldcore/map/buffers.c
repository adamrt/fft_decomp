/* World-map screens clear VRAM and the shared image workspace before drawing new content. */

#include "fft/world_map.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Clear both frame buffers (256 x 480 at the VRAM origin) to black and, when
 * asked, blank the 256x240 backdrop image in g_world_map_scratch_buffer. */
void world_map_image_buffers_clear(s32 clear_scratch) {
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
        for (i = WORLD_MAP_BACKDROP_WORDS - 1; i >= 0; i--) {
            g_world_map_scratch_buffer[i] = 0;
        }
    }
    DrawSync(0);
}
