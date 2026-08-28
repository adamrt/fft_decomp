#include "fft/world.h"
#include "psx/types.h"

void world_menu_draw_text_columns(world_menu_entry_t* entry, s32* row_offset, void* buffer) {
    world_menu_text_layout_t* layout = (world_menu_text_layout_t*)entry->text_binding;
    s32 row;
    s32 column;
    s32 index;

    for (row = 0; row < layout->row_count; row++) {
        g_world_menu_text_state.stride = entry->inner_width;
        for (column = 0; column < 3; column++) {
            index = row + *row_offset;
            if (layout->mode[column] == 2) {
                continue;
            }
            if ((layout->text_ids[column][index] & 0x7FF) == 0x7FF) {
                continue;
            }
            /* The target passes both coordinates without the s16 sign extension. */
            ((void (*)(s32, s32))world_menu_set_text_origin)(layout->x[column], row * 0x10);
            if (layout->mode[column] != 2) {
                g_world_menu_text_color = layout->text_colors[column][index];
            }
            g_world_menu_text_state.color = 0;
            if (g_world_menu_text_color == 4) {
                g_world_menu_text_state.color = 0x44444444;
            }
            if (layout->mode[column] == 0) {
                world_menu_display_text_entry(
                    layout->text_ids[column][index], buffer, &g_world_menu_text_state.origin_x);
            } else if (layout->mode[column] == 1) {
                world_display_menu_number_entry(
                    layout->text_ids[column][index], 2, (s32)buffer, &g_world_menu_text_state.origin_x);
            } else if (layout->mode[column] == 3) {
                world_draw_menu_number_glyphs(
                    layout->text_ids[column][index], 2, buffer, (world_glyph_blit_t*)&g_world_menu_text_state.origin_x);
            }
            g_world_menu_text_color = 0;
            g_world_menu_text_state.color = 0;
        }
    }
}
