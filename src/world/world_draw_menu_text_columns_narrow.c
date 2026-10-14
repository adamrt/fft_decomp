/* Twin of world_screen_menu_draw_text_columns with the stride reduced by 8. */
#include "fft/menu.h"
#include "fft/world.h"
#include "psx/types.h"

void world_draw_menu_text_columns_narrow(menu_entry_t* entry, s32* row_offset, u8* buffer) {
    menu_text_layout_t* layout = entry->context;
    s32 row;
    s32 column;
    s32 index;
    s32* stride_cell = &g_menu_text_state.pen.stride;

    for (row = 0; row < layout->row_count; row++) {
        *stride_cell = entry->inner_width - 8;
        for (column = 0; column < 3; column++) {
            index = row + *row_offset;
            if (layout->mode[column] == MENU_TEXT_COLUMN_HIDDEN) {
                continue;
            }
            if ((layout->text_ids[column][index] & 0x7FF) == 0x7FF) {
                continue;
            }
            menu_set_text_origin(layout->x[column], row * 0x10);
            if (layout->mode[column] != MENU_TEXT_COLUMN_HIDDEN) {
                g_menu_text_palette_offset = layout->text_colors[column][index];
            }
            g_menu_text_state.pen.color = 0;
            if (g_menu_text_palette_offset == 4) {
                g_menu_text_state.pen.color = 0x44444444;
            }
            if (layout->mode[column] == MENU_TEXT_COLUMN_TEXT) {
                menu_display_text_entry(layout->text_ids[column][index], buffer, &g_menu_text_state.pen);
            } else if (layout->mode[column] == MENU_TEXT_COLUMN_NUMBER) {
                menu_display_number_entry(layout->text_ids[column][index], 2, buffer, &g_menu_text_state.pen);
            } else if (layout->mode[column] == MENU_TEXT_COLUMN_NUMBER_GLYPHS) {
                menu_draw_number_glyphs(layout->text_ids[column][index], 2, buffer, &g_menu_text_state.pen);
            }
            g_menu_text_palette_offset = 0;
            g_menu_text_state.pen.color = 0;
        }
    }
}
