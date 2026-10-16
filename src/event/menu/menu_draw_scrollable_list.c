/* Drawing the visible rows of a scrollable list. */

#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/text.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw the visible rows of a scrollable list.
 *
 * The 0x10 render command supplies the list geometry; 0x1C terminates a
 * stream that has no list.
 */
void menu_draw_scrollable_list(const menu_instruction_t* script) {
    RECT rect;
    s16 row_ids[20];
    s32 i;
    s16 visible_rows;
    s16 entry_count;
    s16 selected;
    const menu_list_record_t* list;

    while (script->header.opcode != MENU_SCRIPT_OP_SCROLLABLE_LIST) {
        if (script->header.opcode == VARIANT_MENU_CMD_END_OPCODE) {
            return;
        }
        script = menu_next_const_instruction(script);
    }

    list = &script->list;
    g_menu_list_row_group_count = list->skip_count;
    g_menu_list_row_height = list->row_height;
    visible_rows = list->visible_rows;
    g_menu_list_visible_rows = visible_rows;
    g_menu_list_glyph_width = list->width_a;
    g_menu_list_text_columns = list->width_b;
    g_menu_list_vram_width = (g_menu_list_glyph_width * g_menu_list_text_columns) >> 2;

    entry_count = g_menu_list_entry_count;
    if (g_menu_list_selected_index >= entry_count) {
        g_menu_list_selected_index = entry_count - 1;
    }
    selected = g_menu_list_selected_index;
    if (selected - g_menu_scroll_base_index >= visible_rows) {
        g_menu_scroll_base_index = selected;
    }
    if (selected < g_menu_scroll_base_index) {
        g_menu_scroll_base_index = selected;
    }
    if (entry_count < visible_rows) {
        g_menu_scroll_base_index = 0;
    } else if (entry_count - g_menu_scroll_base_index < visible_rows) {
        g_menu_scroll_base_index = entry_count - visible_rows;
    }

    if (g_menu_list_selected_index >= g_menu_list_entry_count) {
        g_menu_list_selected_index = g_menu_list_entry_count - 1;
    }

    if (g_menu_list_text_table != 0) {
        rect.x = 0x100;
        rect.y = 0x30;
        rect.w = g_menu_list_vram_width;
        rect.h = g_menu_list_visible_rows * 16;
        ClearImage(&rect, 0, 0, 0);
        for (i = 0; i < g_menu_list_visible_rows; i++) {
            row_ids[i] = g_menu_list_entries[i + g_menu_scroll_base_index];
        }
        row_ids[i] = -1;
        rect.x = 0x100;
        rect.y = 0x30;
        rect.w = g_menu_list_vram_width;
        rect.h = g_menu_list_row_height;
        text_render_id_rows_to_vram(g_menu_list_text_table, row_ids, &rect, 0);
    }

    g_menu_list_scroll_direction = 0;
    g_menu_list_redraw_pending = 0;
}
