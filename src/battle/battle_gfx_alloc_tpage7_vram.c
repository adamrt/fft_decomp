#include "fft/battle.h"
#include "psx/types.h"

/* A texture primitive is either a SPRT (code 0x64-0x67) or a POLY_FT4. */
typedef union battle_texture_prim {
    SPRT sprt;
    POLY_FT4 ft4;
} battle_texture_prim_t;

/* Allocate a texture rectangle in the 16-column VRAM allocation grid.
 *
 * The rect size rounds up to 16-pixel cells. The first free position whose
 * cells are all unclaimed is marked 0xff, the primitive UVs/CLUT (and TPage
 * for POLY_FT4) are pointed at it, and rect is rewritten to the 4bpp VRAM
 * area at x 0x1c0 before image (unless -1) is uploaded. A full grid stops
 * this thread. Counterpart of battle_gfx_free_tpage7_vram; WORLD twin world_gfx_alloc_texture_grid_rect. */
void battle_gfx_alloc_tpage7_vram(RECT* rect, battle_texture_prim_t* prim, u32* image) {
    s16 width;
    s16 height;
    s32 width_cells;
    s32 height_cells;
    s32 row;
    s32 column;
    s32 y;
    s32 x;

    width = rect->w;
    height = rect->h;
    width_cells = width >> 4;
    height_cells = height >> 4;
    if (width & 0xF) {
        width_cells++;
    }
    if (height & 0xF) {
        height_cells++;
    }
    for (row = 0; row < 16 - height_cells; row++) {
        for (column = 0; column <= 16 - width_cells; column++) {
            if (g_battle_gfx_tpage7_vram_allocation_grid[row][column] != 0) {
                continue;
            }
            for (y = row; y < row + height_cells; y++) {
                for (x = column; x < column + width_cells; x++) {
                    if (g_battle_gfx_tpage7_vram_allocation_grid[y][x] == 0xFF) {
                        goto next;
                    }
                }
            }
            for (y = row; y < row + height_cells; y++) {
                for (x = column; x < column + width_cells; x++) {
                    g_battle_gfx_tpage7_vram_allocation_grid[y][x] = 0xFF;
                }
            }
            x = column * 16;
            y = row * 16;
            if ((prim->sprt.code & 0xFC) == 0x64) {
                prim->sprt.u0 = x;
                prim->sprt.v0 = y;
                prim->sprt.w = rect->w;
                prim->sprt.h = rect->h;
                prim->sprt.clut = 0x7C3C;
            } else {
                prim->ft4.u0 = prim->ft4.u2 = x;
                prim->ft4.v0 = prim->ft4.v1 = y;
                if (x + rect->w >= 0x100) {
                    x--;
                }
                prim->ft4.u1 = prim->ft4.u3 = rect->w + x;
                prim->ft4.v2 = prim->ft4.v3 = rect->h + y;
                prim->ft4.tpage = GetTPage(0, 0, 0x1C0, 0);
                prim->ft4.clut = 0x7C3C;
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
    battle_thread_exit_current();
}
