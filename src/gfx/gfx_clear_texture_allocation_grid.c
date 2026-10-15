#include "fft/gfx.h"
#include "fft/thread.h"

/* Clear the 15-by-16 allocation grid consumed by the tpage-7 VRAM allocator. */
void gfx_clear_texture_allocation_grid(void) {
    s32 row;
    s32 column;

    for (row = 0; row < 15; row++) {
        for (column = 15; column >= 0; column--) {
            g_gfx_texture_allocation_grid[row][column] = 0;
        }
    }
}
