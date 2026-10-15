#include "fft/gfx.h"
#include "fft/thread.h"

/* Release occupied cells. The rectangle is read before its pointer sentinel
 * is checked; out-of-range positive cell coordinates stop this thread. */
void gfx_free_texture_rect(RECT* rect) {
    s32 x;
    s32 y;
    if (rect->x != -1 && rect != (void*)-1) {
        g_gfx_free_rect_pixels[0] = (rect->x - 0x1c0) * 4;
        g_gfx_free_rect_pixels[1] = rect->y & 0xff;
        g_gfx_free_rect_pixels[2] = rect->w * 4;
        g_gfx_free_rect_pixels[3] = rect->h;
        texture_rect_pixels_to_cells(g_gfx_free_rect_pixels, g_gfx_free_rect_cells);
        for (y = g_gfx_free_rect_cells[1]; y < g_gfx_free_rect_cells[1] + g_gfx_free_rect_cells[3]; y++) {
            for (x = g_gfx_free_rect_cells[0]; x < g_gfx_free_rect_cells[0] + g_gfx_free_rect_cells[2]; x++) {
                if (x >= 16 || y >= 15) {
                    thread_exit_current();
                }
                g_gfx_texture_allocation_grid[y][x] = 0;
            }
        }
    }
}
