#include "fft/event_bunit.h"

/* Draw the visible rows of a scrollable list.
 *
 * The 0x10 render command supplies the list geometry; 0x1C terminates a
 * stream that has no list.
 */
void bunit_menu_draw_scrollable_list(u8* script) {
    RECT rect;
    s16 row_ids[20];
    s32 i;
    s16 visible_rows;
    s16 entry_count;
    s16 selected;

    while (script[0] != 0x10) {
        if (script[0] == 0x1c) {
            return;
        }
        script += script[1];
    }

    g_bunit_menu_list_row_group_count = script[4];
    g_bunit_menu_list_row_height = script[5];
    visible_rows = script[6];
    g_bunit_menu_list_visible_rows = visible_rows;
    g_bunit_menu_list_glyph_width = script[9];
    g_bunit_menu_list_text_columns = script[10];
    g_bunit_menu_list_vram_width = (g_bunit_menu_list_glyph_width * g_bunit_menu_list_text_columns) >> 2;

    entry_count = (s16)g_bunit_menu_list_entry_count;
    if (g_bunit_menu_list_selected_index >= entry_count) {
        g_bunit_menu_list_selected_index = entry_count - 1;
    }
    selected = g_bunit_menu_list_selected_index;
    if (selected - g_bunit_menu_scroll_base_index >= visible_rows) {
        g_bunit_menu_scroll_base_index = selected;
    }
    if (selected < g_bunit_menu_scroll_base_index) {
        g_bunit_menu_scroll_base_index = selected;
    }
    if (entry_count < visible_rows) {
        g_bunit_menu_scroll_base_index = 0;
    } else if (entry_count - g_bunit_menu_scroll_base_index < visible_rows) {
        g_bunit_menu_scroll_base_index = entry_count - visible_rows;
    }

    if (g_bunit_menu_list_selected_index >= (s16)g_bunit_menu_list_entry_count) {
        g_bunit_menu_list_selected_index = (s16)g_bunit_menu_list_entry_count - 1;
    }

    if (g_bunit_menu_list_text_table != 0) {
        rect.x = 0x100;
        rect.y = 0x30;
        rect.w = g_bunit_menu_list_vram_width;
        rect.h = (s32)g_bunit_menu_list_visible_rows * 16;
        ClearImage(&rect, 0, 0, 0);
        for (i = 0; i < g_bunit_menu_list_visible_rows; i++) {
            row_ids[i] = g_bunit_menu_list_entries[i + g_bunit_menu_scroll_base_index];
        }
        row_ids[i] = -1;
        rect.x = 0x100;
        rect.y = 0x30;
        rect.w = g_bunit_menu_list_vram_width;
        rect.h = g_bunit_menu_list_row_height;
        bunit_text_render_id_rows_to_vram((s32)g_bunit_menu_list_text_table, (u16*)row_ids, &rect, 0);
    }

    g_bunit_menu_list_scroll_direction = 0;
    g_bunit_menu_list_redraw_pending = 0;
}
