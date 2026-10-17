/* The two-column unit-panel frame and texture draw modes, with the overlays' empty entry. */

#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* WORLD links the initializer without this empty entry. */
#if defined(FFT_EVENT)
void menu_noop_8(void) {
}
#endif

/* Build the two-column status-panel frame and its texture draw modes. */
void unit_panel_init_primitives(unit_panel_buffer_t* menu) {
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
    menu->tiles[0].h = 0x5a;
    menu->tiles[1].x0 = 0x7c;
    menu->tiles[1].y0 = 1;
    menu->tiles[1].w = 0x10;
    menu->tiles[1].h = 0x5a;
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
    setXY2(&menu->lines[0], 0x10, 1, 0x10, 0x5a);
    setXY2(&menu->lines[1], 0x11, 1, 0x11, 0x5a);
    setXY2(&menu->lines[2], 0x22, 1, 0x22, 0x5a);
    setXY2(&menu->lines[3], 0x23, 1, 0x23, 0x5a);
    setXY2(&menu->lines[4], 0x7a, 1, 0x7a, 0x5a);
    setXY2(&menu->lines[5], 0x7b, 1, 0x7b, 0x5a);
    setXY2(&menu->lines[6], 0x8c, 1, 0x8c, 0x5a);
    setXY2(&menu->lines[7], 0x8d, 1, 0x8d, 0x5a);
    gfx_set_draw_mode_for_texture_page(&menu->frame_draw_mode, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&menu->text_draw_mode, MENU_TEXTURE_PAGE_TEXT_IMAGES);
    gfx_set_draw_mode_for_texture_page(&menu->image_draw_mode, MENU_TEXTURE_PAGE_ITEM_ICONS);
}
