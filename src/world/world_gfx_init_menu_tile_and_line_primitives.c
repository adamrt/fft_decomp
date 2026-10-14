#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Initialize the two-column frame primitives of a menu record. */
void world_gfx_init_menu_tile_and_line_primitives(menu_status_panel_buffer_t* menu) {
    s32 i;

    SetTile(&menu->tiles[0]);
    SetTile(&menu->tiles[1]);
    SetSemiTrans(&menu->tiles[0], 1);
    SetSemiTrans(&menu->tiles[1], 1);
    setRGB0(&menu->tiles[0], 0x30, 0x30, 0x30);
    setRGB0(&menu->tiles[1], 0x30, 0x30, 0x30);
    menu->tiles[0].x0 = 0x12;
    menu->tiles[0].y0 = 1;
    menu->tiles[0].w = 0x10;
    menu->tiles[0].h = 0x5A;
    menu->tiles[1].x0 = 0x7C;
    menu->tiles[1].y0 = 1;
    menu->tiles[1].w = 0x10;
    menu->tiles[1].h = 0x5A;

    for (i = 0; i < 8; i++) {
        SetLineF2(&menu->lines[i]);
        SetSemiTrans(&menu->lines[i], 1);
    }
    setRGB0(&menu->lines[0], 0x10, 0x10, 0x10);
    setRGB0(&menu->lines[1], 0x20, 0x20, 0x20);
    setRGB0(&menu->lines[2], 0x20, 0x20, 0x20);
    setRGB0(&menu->lines[3], 0x10, 0x10, 0x10);
    setRGB0(&menu->lines[4], 0x10, 0x10, 0x10);
    setRGB0(&menu->lines[5], 0x20, 0x20, 0x20);
    setRGB0(&menu->lines[6], 0x20, 0x20, 0x20);
    setRGB0(&menu->lines[7], 0x10, 0x10, 0x10);
    menu->lines[0].x0 = 0x10;
    menu->lines[0].y0 = 1;
    menu->lines[0].x1 = 0x10;
    menu->lines[0].y1 = 0x5A;
    menu->lines[1].x0 = 0x11;
    menu->lines[1].y0 = 1;
    menu->lines[1].x1 = 0x11;
    menu->lines[1].y1 = 0x5A;
    menu->lines[2].x0 = 0x22;
    menu->lines[2].y0 = 1;
    menu->lines[2].x1 = 0x22;
    menu->lines[2].y1 = 0x5A;
    menu->lines[3].x0 = 0x23;
    menu->lines[3].y0 = 1;
    menu->lines[3].x1 = 0x23;
    menu->lines[3].y1 = 0x5A;
    menu->lines[4].x0 = 0x7A;
    menu->lines[4].y0 = 1;
    menu->lines[4].x1 = 0x7A;
    menu->lines[4].y1 = 0x5A;
    menu->lines[5].x0 = 0x7B;
    menu->lines[5].y0 = 1;
    menu->lines[5].x1 = 0x7B;
    menu->lines[5].y1 = 0x5A;
    menu->lines[6].x0 = 0x8C;
    menu->lines[6].y0 = 1;
    menu->lines[6].x1 = 0x8C;
    menu->lines[6].y1 = 0x5A;
    menu->lines[7].x0 = 0x8D;
    menu->lines[7].y0 = 1;
    menu->lines[7].x1 = 0x8D;
    menu->lines[7].y1 = 0x5A;
    world_gfx_set_draw_mode_for_texture_page(&menu->draw_modes[0], MENU_TEXTURE_PAGE_FRAME);
    world_gfx_set_draw_mode_for_texture_page(&menu->draw_modes[1], MENU_TEXTURE_PAGE_TEXT_IMAGES);
    world_gfx_set_draw_mode_for_texture_page(&menu->draw_modes[2], MENU_TEXTURE_PAGE_ITEM_ICONS);
}
