#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Draws a full-screen semi-transparent tile of grey level |intensity|:
 * positive values add (abr 2), negative values subtract (abr 1). */
void world_gfx_draw_screen_tint_tile(world_screen_tint_prims_t* prims, s32 intensity) {
    s32 abr;
    s32 level = intensity;

    if (level != 0) {
        SetTile(&prims->tile);
        SetSemiTrans(&prims->tile, 1);
        abr = 2;
        prims->tile.w = 0x100;
        prims->tile.h = 0xF0;
        prims->tile.r0 = 0;
        prims->tile.g0 = 0;
        prims->tile.b0 = 0;
        prims->tile.x0 = 0x80;
        prims->tile.y0 = 0;
        if (level < 0) {
            abr = 1;
            level = -level;
        }
        SetDrawMode(&prims->mode, 0, 0, (u16)GetTPage(0, abr, 0x3C0, 0x100), &g_world_gfx_texture_window);
        prims->tile.r0 = level;
        prims->tile.g0 = level;
        prims->tile.b0 = level;
        if (g_world_gfx_draw_primitives_immediately != 0) {
            world_gfx_draw_or_append_gpu_primitive(&prims->mode);
            world_gfx_draw_or_append_gpu_primitive(&prims->tile);
        } else {
            world_gfx_draw_or_append_gpu_primitive(&prims->tile);
            world_gfx_draw_or_append_gpu_primitive(&prims->mode);
        }
        g_world_menu_new_button_input = 0;
    }
}
