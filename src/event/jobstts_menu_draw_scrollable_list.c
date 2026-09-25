#include "fft/event_jobstts.h"

/* Draw the visible rows of a scrollable list.
 *
 * The 0x10 render command supplies the list geometry; 0x16 terminates a
 * stream that has no list.
 */
void jobstts_menu_draw_scrollable_list(const u8* script) {
    RECT rect;
    s16 columns[20];
    s32 i;
    s16 visible_rows;
    s16 entry_count;
    s16 selected;

    while (script[0] != 0x10) {
        if (script[0] == 0x16) {
            return;
        }
        script += script[1];
    }

    g_jobstts_menu_list_row_group_count = script[4];
    g_jobstts_menu_list_row_height = script[5];
    visible_rows = script[6];
    g_jobstts_menu_list_visible_rows = visible_rows;
    g_jobstts_menu_list_glyph_width = script[9];
    g_jobstts_menu_list_text_columns = script[10];
    g_jobstts_menu_list_vram_width = (g_jobstts_menu_list_glyph_width * g_jobstts_menu_list_text_columns) >> 2;

    entry_count = g_jobstts_menu_list_entry_count;
    if (g_jobstts_menu_list_selected_index >= entry_count) {
        g_jobstts_menu_list_selected_index = entry_count - 1;
    }
    selected = g_jobstts_menu_list_selected_index;
    if (selected - g_jobstts_menu_scroll_base_index >= visible_rows) {
        g_jobstts_menu_scroll_base_index = selected;
    }
    if (selected < g_jobstts_menu_scroll_base_index) {
        g_jobstts_menu_scroll_base_index = selected;
    }
    if (entry_count < visible_rows) {
        g_jobstts_menu_scroll_base_index = 0;
    } else if (entry_count - g_jobstts_menu_scroll_base_index < visible_rows) {
        g_jobstts_menu_scroll_base_index = entry_count - visible_rows;
    }

    if (g_jobstts_menu_list_selected_index >= g_jobstts_menu_list_entry_count) {
        g_jobstts_menu_list_selected_index = g_jobstts_menu_list_entry_count - 1;
    }

    if (g_jobstts_menu_list_text_table != 0) {
        rect.x = 0x100;
        rect.y = 0x30;
        rect.w = g_jobstts_menu_list_vram_width;
        rect.h = (s32)g_jobstts_menu_list_visible_rows * 16;
        ClearImage(&rect, 0, 0, 0);
        for (i = 0; i < g_jobstts_menu_list_visible_rows; i++) {
            columns[i] = g_jobstts_menu_list_entries[i + g_jobstts_menu_scroll_base_index];
        }
        columns[i] = -1;
        rect.x = 0x100;
        rect.y = 0x30;
        rect.w = g_jobstts_menu_list_vram_width;
        rect.h = g_jobstts_menu_list_row_height;
        jobstts_text_render_id_rows_to_vram((s32)g_jobstts_menu_list_text_table, columns, &rect, 0);
    }

    g_jobstts_menu_list_scroll_direction = 0;
    g_jobstts_menu_list_redraw_pending = 0;
}
