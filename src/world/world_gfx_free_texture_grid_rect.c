#include "fft/world.h"

/*
 * Release a texture rectangle's occupied cells in the 16-by-15 grid.
 *
 * Coordinates and dimensions round up to 16-pixel cells. The target reads
 * rect->x before checking the -1 pointer sentinel; this is not a safe invalid
 * pointer check. Out-of-range positive cell coordinates stop this thread.
 */
void world_gfx_free_texture_grid_rect(RECT* rect) {
    s32* source;
    s32* destination;
    s32 i;
    s32 x;
    s32 y;
    s32 value;
    if (rect->x != -1 && rect != (void*)-1) {
        g_world_gfx_free_rect_texels[0] = (rect->x - 0x1c0) * 4;
        g_world_gfx_free_rect_texels[1] = (u8)rect->y;
        source = g_world_gfx_free_rect_texels;
        i = 0;
        g_world_gfx_free_rect_texels[2] = rect->w * 4;
        g_world_gfx_free_rect_texels[3] = rect->h;
        destination = g_world_gfx_free_rect_grid_cells;
        for (; i < 4; i++) {
            value = *source >> 4;
            *destination = value;
            if (*source & 15) {
                *destination = value + 1;
            }
            source++;
            destination++;
        }
        for (y = g_world_gfx_free_rect_grid_cells[1];
            y < g_world_gfx_free_rect_grid_cells[1] + g_world_gfx_free_rect_grid_cells[3]; y++) {
            for (x = g_world_gfx_free_rect_grid_cells[0];
                x < g_world_gfx_free_rect_grid_cells[0] + g_world_gfx_free_rect_grid_cells[2]; x++) {
                if (x >= 16 || y >= 15) {
                    world_thread_exit_current();
                }
                g_world_gfx_texture_allocation_grid[y][x] = 0;
            }
        }
    }
}
