#include "fft/bunit.h"
#include "fft/equip.h"

/* Draw the visible rows of a scrollable list.
 *
 * The 0x10 render command supplies the list geometry; 0x19 terminates a
 * stream that has no list.
 */
void equip_menu_draw_scrollable_list(const u8* script) {
    RECT rect;
    s16 row_ids[20];
    s32 i;
    s16 visible_rows;
    s16 entry_count;
    s16 selected;

    while (script[0] != 0x10) {
        if (script[0] == 0x19) {
            return;
        }
        script += script[1];
    }

    g_equip_menu_list_row_group_count = script[4];
    g_equip_menu_list_row_height = script[5];
    visible_rows = script[6];
    g_equip_menu_list_visible_rows = visible_rows;
    g_equip_menu_list_glyph_width = script[9];
    g_equip_menu_list_text_columns = script[10];
    g_equip_menu_list_vram_width = (g_equip_menu_list_glyph_width * g_equip_menu_list_text_columns) >> 2;

    entry_count = g_equip_menu_list_entry_count;
    if ((s16)g_equip_menu_selected_list_index >= entry_count) {
        g_equip_menu_selected_list_index = entry_count - 1;
    }
    selected = (s16)g_equip_menu_selected_list_index;
    if (selected - g_equip_menu_scroll_base_index >= visible_rows) {
        g_equip_menu_scroll_base_index = selected;
    }
    if (selected < g_equip_menu_scroll_base_index) {
        g_equip_menu_scroll_base_index = selected;
    }
    if (entry_count < visible_rows) {
        g_equip_menu_scroll_base_index = 0;
    } else if (entry_count - g_equip_menu_scroll_base_index < visible_rows) {
        g_equip_menu_scroll_base_index = entry_count - visible_rows;
    }

    if ((s16)g_equip_menu_selected_list_index >= g_equip_menu_list_entry_count) {
        g_equip_menu_selected_list_index = g_equip_menu_list_entry_count - 1;
    }

    if (g_equip_menu_list_text_table != 0) {
        rect.x = 0x100;
        rect.y = 0x30;
        rect.w = g_equip_menu_list_vram_width;
        rect.h = (s32)g_equip_menu_list_visible_rows * 16;
        ClearImage(&rect, 0, 0, 0);
        for (i = 0; i < g_equip_menu_list_visible_rows; i++) {
            row_ids[i] = g_equip_menu_list_entries[i + g_equip_menu_scroll_base_index];
        }
        row_ids[i] = -1;
        rect.x = 0x100;
        rect.y = 0x30;
        rect.w = g_equip_menu_list_vram_width;
        rect.h = (u16)g_equip_menu_list_row_height;
        equip_text_render_id_rows_to_vram(g_equip_menu_list_text_table, row_ids, &rect, 0);
    }

    g_equip_menu_list_scroll_direction = 0;
    g_equip_menu_list_redraw_pending = 0;
}
