#include "fft/battle_map.h"
#include "psx/types.h"

/* Load the sixteen colors of one CLUT into a map CLUT row: the upload staging
 * (g_map_palette_modified_colors), the row's packed mirror, and its
 * decoded 5-bit components and alpha bit. */
void map_extract_palette_colors(const palette_t palette, s32 clut_row, s32 clut_index) {
    s32 color_index;
    s32 selected_clut;
    s32 i;
    const u16* palette_ptr;
    u16* upload_palette_base;
    u16* upload_row;
    u16* packed_row;
    map_palette_color_components_t* component_row;
    u16 color;
    map_palette_color_components_t* components;

    selected_clut = clut_index;
    i = 0;
    component_row = g_map_palette_state.clut_rows[clut_row].components;
    upload_palette_base = g_map_palette_modified_colors[0];
    upload_row = upload_palette_base + clut_row * MAP_CLUT_ROW_COLOR_COUNT;
    packed_row = g_map_palette_state.clut_rows[clut_row].packed_colors;
    /* Named color fields reload across the stores; decode each packed 16-bit value once. */
    palette_ptr = (const u16*)palette;
    do {
        /* A CLUT entry is ABBBBBGGGGGRRRRR. */
        color = *palette_ptr++;
        /* Multiplication folds this into the loop and removes four target bytes. */
        color_index = selected_clut << 4;
        color_index += i;
        i++;
        upload_row[color_index] = color;
        components = &component_row[color_index];
        packed_row[color_index] = color;
        components->color.r = color & 0x1f;
        components->color.g = (color & 0x3e0) >> 5;
        components->color.b = (color & 0x7c00) >> 10;
        components->semi_transparent = color >> 15;
    } while (i < MAP_CLUT_COLOR_COUNT);
}
