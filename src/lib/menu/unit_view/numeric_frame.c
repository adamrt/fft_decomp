/* Numeric-panel frame geometry, palette selection and primitive submission. */

#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/status_screen.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern RECT g_status_screen_numbers_texture_window; /* texture window for the menu page */

/* Menu palette color of each of the numeric display frame's twelve lines. */
static u8 g_status_screen_numbers_palette_indices[12] = { 1, 1, 15, 7, 3, 2, 7, 14, 13, 13, 13, 13 };

/* Eight-byte line placements: signed endpoint offsets and width/height selectors.
 * Byte-offset indexing preserves the native loop; typed row indexing changes it. */
typedef enum menu_line_layout_offset {
    MENU_LINE_LAYOUT_X0 = 0,
    MENU_LINE_LAYOUT_Y0 = 1,
    MENU_LINE_LAYOUT_X1 = 2,
    MENU_LINE_LAYOUT_Y1 = 3,
    MENU_LINE_LAYOUT_ADD_W0 = 4,
    MENU_LINE_LAYOUT_ADD_H0 = 5,
    MENU_LINE_LAYOUT_ADD_W1 = 6,
    MENU_LINE_LAYOUT_ADD_H1 = 7,
    MENU_LINE_LAYOUT_SIZE = 8
} menu_line_layout_offset_e;

static s8 g_status_screen_numbers_frame_layout[12 * MENU_LINE_LAYOUT_SIZE]
    = { 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, -1, 0, 0, 0, 1, 0, 1, -2, 1, 0, 0, 1, 0, 1, 1, 1, -2, 0, 0, 0, 1, 1, -1, 1, -1,
          0, 1, 0, 1, -1, 2, -1, -2, 1, 0, 1, 1, -1, 1, -1, 1, 1, 0, 1, 0, 1, -1, -1, -1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1,
          1, 1, 1, 2, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, -1, 1, 0, 1, 1 };

/* Build the numeric display frame around box: twelve lines placed by
 * g_status_screen_numbers_frame_layout relative to the box's corners and colored from
 * the normal menu palette, the two draw modes and a sprite covering the box. */
void status_screen_init_numbers_frame(RECT* box, menu_numeric_frame_t* menu) {
    LINE_F2* line = menu->lines;
    s32 i;
    s32 offset;

    for (i = 0; i < 12; i++) {
        SetLineF2(line);
        line++;
    }
    menu_init_primitive_colors_normal(menu);
    line = menu->lines;
    offset = 0;
    for (i = 0; i < 12; i++) {
        line->x0 = g_status_screen_numbers_frame_layout[offset + MENU_LINE_LAYOUT_X0] + box->x;
        line->y0 = g_status_screen_numbers_frame_layout[offset + MENU_LINE_LAYOUT_Y0] + box->y;
        line->x1 = g_status_screen_numbers_frame_layout[offset + MENU_LINE_LAYOUT_X1] + box->x;
        line->y1 = g_status_screen_numbers_frame_layout[offset + MENU_LINE_LAYOUT_Y1] + box->y;
        if (g_status_screen_numbers_frame_layout[offset + MENU_LINE_LAYOUT_ADD_W0] != 0) {
            line->x0 += box->w;
        }
        if (g_status_screen_numbers_frame_layout[offset + MENU_LINE_LAYOUT_ADD_H0] != 0) {
            line->y0 += box->h;
        }
        if (g_status_screen_numbers_frame_layout[offset + MENU_LINE_LAYOUT_ADD_W1] != 0) {
            line->x1 += box->w;
        }
        if (g_status_screen_numbers_frame_layout[offset + MENU_LINE_LAYOUT_ADD_H1] != 0) {
            line->y1 += box->h;
        }
        offset += MENU_LINE_LAYOUT_SIZE;
        line++;
    }
    SetDrawMode(&menu->draw_mode_menu, 1, 0, GetTPage(0, 0, 0x3c0, 0x100), &g_status_screen_numbers_texture_window);
    SetDrawMode(&menu->draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);
    menu_init_sprt(&menu->sprite);
    menu->sprite.x0 = box->x;
    menu->sprite.y0 = box->y;
    menu->sprite.w = box->w;
    menu->sprite.h = box->h;
}

/* Submit the frame's twelve lines, sprite and draw modes. */
void status_screen_submit_numbers_frame(menu_numeric_frame_t* display) {
    s32 index;

    for (index = 0; index < 12; index++) {
        gfx_draw_or_append_gpu_primitive(&display->lines[index]);
    }
    gfx_draw_or_append_gpu_primitive(&display->draw_mode);
    gfx_draw_or_append_gpu_primitive(&display->sprite);
    gfx_draw_or_append_gpu_primitive(&display->draw_mode_menu);
}

/* Give the twelve frame lines the colors of the normal menu palette (the first
 * sixteen of the menu palette colors) and the sprite the normal menu CLUT, GetClut(0x3c0, 0x1f0). */
void menu_init_primitive_colors_normal(menu_numeric_frame_t* menu) {
    s32 index;
    s32 palette_index;
    LINE_F2* line;

    for (index = 0, line = menu->lines; index < 12; index++, line++) {
        palette_index = g_status_screen_numbers_palette_indices[index];
        line->r0 = g_menu_palette_colors.colors[palette_index].red << 3;
        line->g0 = g_menu_palette_colors.colors[palette_index].green << 3;
        line->b0 = g_menu_palette_colors.colors[palette_index].blue << 3;
    }
    menu->sprite.clut = MENU_CLUT_NORMAL;
}

/* Give the twelve frame lines the colors of the dimmed menu palette (the second
 * sixteen of the menu palette colors) and the sprite the dimmed menu CLUT, GetClut(0x3c0, 0x1f4). */
void menu_init_primitive_colors_dimmed(menu_numeric_frame_t* menu) {
    s32 index;
    s32 palette_index;
    LINE_F2* line;

    for (index = 0, line = menu->lines; index < 12; index++, line++) {
        palette_index = (g_status_screen_numbers_palette_indices[index] + PALETTE_COLOR_COUNT);
        line->r0 = g_menu_palette_colors.colors[palette_index].red << 3;
        line->g0 = g_menu_palette_colors.colors[palette_index].green << 3;
        line->b0 = g_menu_palette_colors.colors[palette_index].blue << 3;
    }
    menu->sprite.clut = MENU_CLUT_DIMMED;
}
