#include "fft/event_attack.h"
#include "psx/gpu.h"
#include "psx/types.h"

void attack_gfx_init_menu_tile_and_line_primitives(battle_menu_status_panel_buffer_t* menu) {
    s32 i;

    SetTile(&menu->tiles[0]);
    SetTile(&menu->tiles[1]);
    SetSemiTrans(&menu->tiles[0], 1);
    SetSemiTrans(&menu->tiles[1], 1);
    menu->tiles[0].r0 = 0x30;
    menu->tiles[0].g0 = 0x30;
    menu->tiles[0].b0 = 0x30;
    menu->tiles[1].r0 = 0x30;
    menu->tiles[1].g0 = 0x30;
    menu->tiles[1].b0 = 0x30;
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
    menu->lines[0].r0 = 0x10;
    menu->lines[0].g0 = 0x10;
    menu->lines[0].b0 = 0x10;
    menu->lines[1].r0 = 0x20;
    menu->lines[1].g0 = 0x20;
    menu->lines[1].b0 = 0x20;
    menu->lines[2].r0 = 0x20;
    menu->lines[2].g0 = 0x20;
    menu->lines[2].b0 = 0x20;
    menu->lines[3].r0 = 0x10;
    menu->lines[3].g0 = 0x10;
    menu->lines[3].b0 = 0x10;
    menu->lines[4].r0 = 0x10;
    menu->lines[4].g0 = 0x10;
    menu->lines[4].b0 = 0x10;
    menu->lines[5].r0 = 0x20;
    menu->lines[5].g0 = 0x20;
    menu->lines[5].b0 = 0x20;
    menu->lines[6].r0 = 0x20;
    menu->lines[6].g0 = 0x20;
    menu->lines[6].b0 = 0x20;
    menu->lines[7].r0 = 0x10;
    menu->lines[7].g0 = 0x10;
    menu->lines[7].b0 = 0x10;
    menu->lines[0].x0 = 0x10;
    menu->lines[0].y0 = 1;
    menu->lines[0].x1 = 0x10;
    menu->lines[0].y1 = 0x5a;
    menu->lines[1].x0 = 0x11;
    menu->lines[1].y0 = 1;
    menu->lines[1].x1 = 0x11;
    menu->lines[1].y1 = 0x5a;
    menu->lines[2].x0 = 0x22;
    menu->lines[2].y0 = 1;
    menu->lines[2].x1 = 0x22;
    menu->lines[2].y1 = 0x5a;
    menu->lines[3].x0 = 0x23;
    menu->lines[3].y0 = 1;
    menu->lines[3].x1 = 0x23;
    menu->lines[3].y1 = 0x5a;
    menu->lines[4].x0 = 0x7a;
    menu->lines[4].y0 = 1;
    menu->lines[4].x1 = 0x7a;
    menu->lines[4].y1 = 0x5a;
    menu->lines[5].x0 = 0x7b;
    menu->lines[5].y0 = 1;
    menu->lines[5].x1 = 0x7b;
    menu->lines[5].y1 = 0x5a;
    menu->lines[6].x0 = 0x8c;
    menu->lines[6].y0 = 1;
    menu->lines[6].x1 = 0x8c;
    menu->lines[6].y1 = 0x5a;
    menu->lines[7].x0 = 0x8d;
    menu->lines[7].y0 = 1;
    menu->lines[7].x1 = 0x8d;
    menu->lines[7].y1 = 0x5a;
    battle_gfx_set_draw_mode_for_texture_page(&menu->draw_modes[0], 0);
    battle_gfx_set_draw_mode_for_texture_page(&menu->draw_modes[1], 2);
    battle_gfx_set_draw_mode_for_texture_page(&menu->draw_modes[2], 4);
}
