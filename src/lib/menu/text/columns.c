/* Drawing the visible text and number entries of a page's three columns. */

#include "fft/menu/menu.h"
#include "fft/menu/menu_list.h"
#include "fft/menu/menu_text.h"
#include "fft/text/text.h"
#include "psx/types.h"

/* Draw the visible text or number entries in a page's three columns. */
void menu_draw_text_columns(menu_entry_t* entry, s32* row_offset, u8* buffer) {
    menu_text_layout_t* layout = entry->context;
    s32 row;
    s32 column;
    s32 index;

    for (row = 0; row < layout->row_count; row++) {
        g_menu_window_text_pen.stride = entry->inner_width;
        for (column = 0; column < 3; column++) {
            index = row + *row_offset;
            if (layout->mode[column] == MENU_TEXT_COLUMN_HIDDEN) {
                continue;
            }
            if ((layout->text_ids[column][index] & TEXT_ID_ENTRY_MASK) == TEXT_ID_ENTRY_MASK) {
                continue;
            }
            menu_text_set_origin(layout->x[column], row * 16);
            /* Always true after the test above, but the bytes keep it. */
            if (layout->mode[column] != MENU_TEXT_COLUMN_HIDDEN) {
                g_menu_text_palette_offset = layout->text_colors[column][index];
            }
            /* A row drawn in palette 4 (the highlighted row) also gets a background of
             * color index 4 in every pixel nibble. */
            g_menu_window_text_pen.color = 0;
            if (g_menu_text_palette_offset == 4) {
                g_menu_window_text_pen.color = 0x44444444;
            }
            if (layout->mode[column] == MENU_TEXT_COLUMN_TEXT) {
                text_render_entry_to_image(layout->text_ids[column][index], buffer, &g_menu_window_text_pen);
            } else if (layout->mode[column] == MENU_TEXT_COLUMN_NUMBER) {
                text_draw_number(layout->text_ids[column][index], 2, buffer, &g_menu_window_text_pen);
            } else if (layout->mode[column] == MENU_TEXT_COLUMN_NUMBER_GLYPHS) {
                text_draw_large_number_glyphs(layout->text_ids[column][index], 2, buffer, &g_menu_window_text_pen);
            }
            g_menu_text_palette_offset = 0;
            g_menu_window_text_pen.color = 0;
        }
    }
}
