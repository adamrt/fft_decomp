/* Allocate and release menu texture rectangles in the 16-by-15 VRAM cell grid. */

#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Independent 15-row, 16-column grids for BATTLE and WORLD. */
extern u8 g_gfx_texture_allocation_grid[15][16];

/* Texture allocator scratch rectangles, in pixels and allocation cells. */
extern s32 g_gfx_free_rect_pixels[4];
extern s32 g_gfx_free_rect_cells[4];

/* Free every cell of the 16 x 15 texture allocation grid that gfx_alloc_texture_rect claims from. */
void gfx_clear_texture_allocation_grid(void) {
    s32 row;
    s32 column;

    for (row = 0; row < 15; row++) {
        for (column = 15; column >= 0; column--) {
            g_gfx_texture_allocation_grid[row][column] = 0;
        }
    }
}

/* Allocate a texture rectangle in the 16-column VRAM allocation grid.
 *
 * The rect size rounds up to 16-pixel cells. The first free position whose
 * cells are all unclaimed is marked 0xff, the primitive UVs/CLUT (and TPage
 * for POLY_FT4) are pointed at it, and rect is rewritten to the 4bpp VRAM
 * area at x 0x1c0 before image (unless -1) is uploaded. A full grid stops
 * this thread. The allocation grid is local to the active scene. */
void gfx_alloc_texture_rect(RECT* rect, void* prim, u32* image) {
    s16 width = rect->w;
    s16 height = rect->h;
    s32 width_cells = width >> 4;
    s32 height_cells = height >> 4;
    s32 row;
    s32 column;
    s32 y;
    s32 x;

    if (width & 0xF) {
        width_cells++;
    }
    if (height & 0xF) {
        height_cells++;
    }
    for (row = 0; row < 16 - height_cells; row++) {
        for (column = 0; column <= 16 - width_cells; column++) {
            if (g_gfx_texture_allocation_grid[row][column] != 0) {
                continue;
            }
            for (y = row; y < row + height_cells; y++) {
                for (x = column; x < column + width_cells; x++) {
                    if (g_gfx_texture_allocation_grid[y][x] == 0xFF) {
                        goto next;
                    }
                }
            }
            for (y = row; y < row + height_cells; y++) {
                for (x = column; x < column + width_cells; x++) {
                    g_gfx_texture_allocation_grid[y][x] = 0xFF;
                }
            }
            x = column * 16;
            y = row * 16;
            /* GPU codes 0x64-0x67 are the free-size sprite (SPRT). */
            if ((getcode(prim) & 0xFC) == 0x64) {
                SPRT* sprite = prim;

                sprite->u0 = x;
                sprite->v0 = y;
                sprite->w = rect->w;
                sprite->h = rect->h;
                sprite->clut = MENU_CLUT_NORMAL;
            } else {
                POLY_FT4* quad = prim;

                quad->u0 = quad->u2 = x;
                quad->v0 = quad->v1 = y;
                /* Keep the right edge u inside the 256-pixel page. */
                if (x + rect->w >= 0x100) {
                    x--;
                }
                quad->u1 = quad->u3 = rect->w + x;
                quad->v2 = quad->v3 = rect->h + y;
                quad->tpage = GetTPage(0, 0, 0x1C0, 0);
                quad->clut = MENU_CLUT_NORMAL;
            }
            rect->x = column * 4 + 0x1C0;
            rect->y = y;
            rect->w = (rect->w & 3) ? (rect->w >> 2) + 1 : rect->w >> 2;
            if (image != (u32*)-1) {
                LoadImage(rect, image);
            }
            return;
        next:;
        }
    }
    thread_exit_current();
}

/* Release occupied cells. The rectangle is read before its pointer sentinel
 * is checked; out-of-range positive cell coordinates stop this thread. */
void gfx_free_texture_rect(RECT* rect) {
    s32 x;
    s32 y;
    s32 i;
    s32 value;
    if (rect->x != -1 && rect != (void*)-1) {
        g_gfx_free_rect_pixels[0] = (rect->x - 0x1c0) * 4;
        g_gfx_free_rect_pixels[1] = rect->y & 0xff;
        g_gfx_free_rect_pixels[2] = rect->w * 4;
        g_gfx_free_rect_pixels[3] = rect->h;
        for (i = 0; i < 4; i++) {
            value = g_gfx_free_rect_pixels[i] >> 4;
            g_gfx_free_rect_cells[i] = value;
            if (g_gfx_free_rect_pixels[i] & 0xf) {
                g_gfx_free_rect_cells[i] = value + 1;
            }
        }
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
