#include "fft/menu.h"
#include "psx/types.h"

/* Give the twelve frame lines the colors of the dimmed menu palette (the second
 * sixteen of the menu palette colors) and the sprite the dimmed menu CLUT, GetClut(0x3c0, 0x1f4). */
void menu_init_primitive_colors_dimmed(menu_palette_primitives_t* menu) {
    s32 index;
    s32 palette_index;
    LINE_F2* line;

    for (index = 0, line = menu->lines; index < 12; index++, line++) {
        palette_index = (g_menu_numeric_display_palette_indices[index] + PALETTE_COLOR_COUNT);
        line->r0 = g_menu_palette_colors.colors[palette_index].red << 3;
        line->g0 = g_menu_palette_colors.colors[palette_index].green << 3;
        line->b0 = g_menu_palette_colors.colors[palette_index].blue << 3;
    }
    menu->sprite.clut = 0x7d3c;
}
