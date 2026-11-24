/* The two-column unit-panel frame and texture draw modes, with the overlays' empty entry. */

#include "fft/gfx/gfx_primitive.h"
#include "fft/menu/menu_gfx.h"
#include "fft/party/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* WORLD links the initializer without this empty entry. */
#if defined(FFT_EVENT)
void menu_noop_8(void) {
}
#endif

/* Build the two-column status-panel frame and its texture draw modes. */
void unit_panel_init_primitives(unit_panel_frame_t* menu) {
    s32 i;

    SetTile(&menu->tiles[0]);
    SetTile(&menu->tiles[1]);
    SetSemiTrans(&menu->tiles[0], 1);
    SetSemiTrans(&menu->tiles[1], 1);
    setRGB0(&menu->tiles[0], 0x30, 0x30, 0x30);
    setRGB0(&menu->tiles[1], 0x30, 0x30, 0x30);
    menu->tiles[0].x0 = 18;
    menu->tiles[0].y0 = 1;
    menu->tiles[0].w = 16;
    menu->tiles[0].h = 90;
    menu->tiles[1].x0 = 124;
    menu->tiles[1].y0 = 1;
    menu->tiles[1].w = 16;
    menu->tiles[1].h = 90;
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
    setXY2(&menu->lines[0], 16, 1, 16, 90);
    setXY2(&menu->lines[1], 17, 1, 17, 90);
    setXY2(&menu->lines[2], 34, 1, 34, 90);
    setXY2(&menu->lines[3], 35, 1, 35, 90);
    setXY2(&menu->lines[4], 122, 1, 122, 90);
    setXY2(&menu->lines[5], 123, 1, 123, 90);
    setXY2(&menu->lines[6], 140, 1, 140, 90);
    setXY2(&menu->lines[7], 141, 1, 141, 90);
    gfx_set_draw_mode_for_texture_page(&menu->frame_draw_mode, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&menu->text_draw_mode, MENU_TEXTURE_PAGE_TEXT_IMAGES);
    gfx_set_draw_mode_for_texture_page(&menu->image_draw_mode, MENU_TEXTURE_PAGE_ITEM_ICONS);
}
