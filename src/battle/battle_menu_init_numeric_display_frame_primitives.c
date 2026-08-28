#include "fft/battle.h"
#include "fft/menu.h"

enum battle_menu_line_layout_offset {
    BATTLE_MENU_LINE_LAYOUT_X0 = 0,
    BATTLE_MENU_LINE_LAYOUT_Y0 = 1,
    BATTLE_MENU_LINE_LAYOUT_X1 = 2,
    BATTLE_MENU_LINE_LAYOUT_Y1 = 3,
    BATTLE_MENU_LINE_LAYOUT_ADD_W0 = 4,
    BATTLE_MENU_LINE_LAYOUT_ADD_H0 = 5,
    BATTLE_MENU_LINE_LAYOUT_ADD_W1 = 6,
    BATTLE_MENU_LINE_LAYOUT_ADD_H1 = 7,
    BATTLE_MENU_LINE_LAYOUT_SIZE = 8,
};

/* Build the numeric-display frame primitives for `bounds`.
 *
 * The WORLD twin establishes the packet layout: 2 draw modes, 12
 * line primitives, and a trailing sprite. The signed layout bytes select each
 * endpoint and whether it receives the rectangle width or height.
 */
void battle_menu_init_numeric_display_frame_primitives(RECT* bounds, world_menu_palette_primitives_t* menu) {
    world_menu_line_f2_t* line;
    s32 index;
    s32 offset;

    line = menu->lines;
    for (index = 0; index < 12; index++) {
        SetLineF2(line);
        line++;
    }

    battle_menu_init_primitive_colors_palette_bank_0(menu);
    line = menu->lines;
    offset = 0;
    for (index = 0; index < 12; index++) {
        line->x0 = ((s8*)g_battle_menu_numeric_display_frame_offsets)[offset + BATTLE_MENU_LINE_LAYOUT_X0] + bounds->x;
        line->y0 = ((s8*)g_battle_menu_numeric_display_frame_offsets)[offset + BATTLE_MENU_LINE_LAYOUT_Y0] + bounds->y;
        line->x1 = ((s8*)g_battle_menu_numeric_display_frame_offsets)[offset + BATTLE_MENU_LINE_LAYOUT_X1] + bounds->x;
        line->y1 = ((s8*)g_battle_menu_numeric_display_frame_offsets)[offset + BATTLE_MENU_LINE_LAYOUT_Y1] + bounds->y;
        if (((s8*)g_battle_menu_numeric_display_frame_offsets)[offset + BATTLE_MENU_LINE_LAYOUT_ADD_W0] != 0) {
            line->x0 += bounds->w;
        }
        if (((s8*)g_battle_menu_numeric_display_frame_offsets)[offset + BATTLE_MENU_LINE_LAYOUT_ADD_H0] != 0) {
            line->y0 += bounds->h;
        }
        if (((s8*)g_battle_menu_numeric_display_frame_offsets)[offset + BATTLE_MENU_LINE_LAYOUT_ADD_W1] != 0) {
            line->x1 += bounds->w;
        }
        if (((s8*)g_battle_menu_numeric_display_frame_offsets)[offset + BATTLE_MENU_LINE_LAYOUT_ADD_H1] != 0) {
            line->y1 += bounds->h;
        }
        offset += BATTLE_MENU_LINE_LAYOUT_SIZE;
        line++;
    }

    SetDrawMode(
        &menu->draw_mode_menu, 1, 0, (u16)GetTPage(0, 0, 0x3c0, 0x100), &g_battle_menu_numeric_display_texture_window);
    SetDrawMode(&menu->draw_mode, 0, 0, (u16)GetTPage(0, 2, 0x3c0, 0x100), &g_battle_menu_disabled_texture_window);
    battle_menu_init_semitransparent_sprt(&menu->sprite);
    menu->sprite.x0 = bounds->x;
    menu->sprite.y0 = bounds->y;
    menu->sprite.w = bounds->w;
    menu->sprite.h = bounds->h;
}
