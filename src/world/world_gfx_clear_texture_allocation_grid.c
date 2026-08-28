#include "fft/world.h"
#include "psx/types.h"

void world_gfx_clear_texture_allocation_grid(void) {
    s32 row;
    s32 column;

    for (row = 0; row < 15; row++) {
        for (column = 15; column >= 0; column--) {
            g_world_gfx_texture_allocation_grid[row][column] = 0;
        }
    }
}
