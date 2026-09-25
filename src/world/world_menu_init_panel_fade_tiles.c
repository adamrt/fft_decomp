#include "fft/menu_types.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

typedef struct world_menu_fade_tile_frame_split {
    DR_MODE draw_mode;
    TILE center;
    TILE backdrop[9];
    TILE bar[9];
} world_menu_fade_tile_frame_split_t;

/* Initialize the center tile and two nine-tile scanline strips. */
void world_menu_init_panel_fade_tiles(void) {
    world_menu_fade_tile_frame_split_t* frame;
    s32 i;
    /* GetTPage's argument registers, tied by the barrier below. */
    register s32 mode __asm__("$4") = 0;
    register s32 blend __asm__("$5") = 2;
    register s32 x __asm__("$6") = 0x3c0;
    register s32 y __asm__("$7") = 0x100;

    frame = (world_menu_fade_tile_frame_split_t*)&g_world_menu_panel_fade_frames[0];

    /* Materializes the frame base before the register saves and the GetTPage
     * call; without it GCC sets up the base after the call. */
    __asm__("" : "=r"(mode), "=r"(blend), "=r"(x), "=r"(y) : "0"(mode), "1"(blend), "2"(x), "3"(y), "r"(frame));
    SetDrawMode(&frame->draw_mode, 0, 0, GetTPage(mode, blend, x, y), &g_world_gfx_texture_window);

    SetTile(&frame->center);
    frame->center.r0 = 0;
    frame->center.g0 = 0;
    frame->center.b0 = 0;
    SetSemiTrans(&frame->center, 1);
    frame->center.x0 = 0x80;
    frame->center.y0 = 0xac;
    frame->center.w = 0x100;
    frame->center.h = 0x32;

    for (i = 0; i < 9; i++) {
        SetTile(&frame->backdrop[i]);
        frame->backdrop[i].r0 = 0;
        frame->backdrop[i].g0 = 0;
        frame->backdrop[i].b0 = 0;
        SetSemiTrans(&frame->backdrop[i], 1);
        frame->backdrop[i].w = 0x100;
        frame->backdrop[i].h = 1;
        frame->backdrop[i].x0 = 0x80;
        frame->backdrop[i].y0 = i + 0xa3;

        SetTile(&frame->bar[i]);
        frame->bar[i].r0 = 0;
        frame->bar[i].g0 = 0;
        frame->bar[i].b0 = 0;
        SetSemiTrans(&frame->bar[i], 1);
        frame->bar[i].w = 0x100;
        frame->bar[i].h = 1;
        frame->bar[i].x0 = 0x80;
        frame->bar[i].y0 = i + 0xde;
    }

    world_script_copy_bytes(&g_world_menu_panel_fade_frames[1], &g_world_menu_panel_fade_frames[0],
        sizeof(world_menu_fade_tile_frame_split_t));
}
