#include "fft/world.h"

/* Give the twelve frame lines the colors of the normal menu palette (the first
 * sixteen of the menu palette colors) and the sprite the normal menu CLUT, GetClut(0x3c0, 0x1f0). */
void world_menu_init_primitive_colors_normal(menu_palette_primitives_t* menu) {
    s32 index;
    s32 palette_index;
    LINE_F2* line;

    line = menu->lines;
    for (index = 0; index < 12; index++) {
        palette_index = g_world_numeric_display_palette_indices[index];
        line->r0 = g_world_menu_palette_colors.colors[palette_index].red << 3;
        line->g0 = g_world_menu_palette_colors.colors[palette_index].green << 3;
        line->b0 = g_world_menu_palette_colors.colors[palette_index].blue << 3;
        line++;
    }
    menu->sprite.clut = 0x7c3c;
}
