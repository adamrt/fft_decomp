#include "fft/open.h"
#include "psx/libgpu.h"

/* Copy the 64x256 VRAM page kept at (960, 0) back to (960, 256), where
 * open_movie_init_and_play_end saved it from before the ending's picture went
 * over it. */
void open_gfx_restore_frame_vram_page(void) {
    RECT source;

    source.x = 0x3c0;
    source.y = 0;
    source.w = 0x40;
    source.h = 0x100;
    MoveImage(&source, 0x3c0, 0x100);
    DrawSync(0);
}
