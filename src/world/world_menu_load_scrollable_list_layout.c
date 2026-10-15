#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Find the next list-layout record in a menu script stream, apply its
 * geometry, clamp the cursor and scroll position to the option count, and
 * redraw the visible text ids when a list font is active. */
void world_menu_load_scrollable_list_layout(menu_instruction_t* script) {
    RECT rect;
    s16 text_ids[20];
    menu_list_record_t* record;
    menu_instruction_t* instruction = script;
    s32 i;

    while (instruction->header.opcode != MENU_SCRIPT_OP_SCROLLABLE_LIST) {
        if (instruction->header.opcode == WORLD_MENU_SCRIPT_END) {
            return;
        }
        instruction = menu_next_instruction(instruction);
    }
    record = &instruction->list;
    g_world_menu_list_row_record_count = record->skip_count;
    g_world_menu_row_height = record->row_height;
    g_world_menu_list_visible_rows = record->visible_rows;
    g_world_menu_list_glyph_width = record->width_a;
    g_world_menu_list_text_columns = record->width_b;
    g_world_menu_list_image_width = (g_world_menu_list_glyph_width * g_world_menu_list_text_columns) >> 2;
    if (g_menu_list_selected_index >= g_menu_list_entry_count) {
        g_menu_list_selected_index = g_menu_list_entry_count - 1;
    }
    if (g_menu_list_selected_index - g_menu_scroll_base_index >= g_world_menu_list_visible_rows) {
        g_menu_scroll_base_index = g_menu_list_selected_index;
    }
    if (g_menu_list_selected_index < g_menu_scroll_base_index) {
        g_menu_scroll_base_index = g_menu_list_selected_index;
    }
    if (g_menu_list_entry_count < g_world_menu_list_visible_rows) {
        g_menu_scroll_base_index = 0;
    } else if (g_menu_list_entry_count - g_menu_scroll_base_index < g_world_menu_list_visible_rows) {
        g_menu_scroll_base_index = g_menu_list_entry_count - g_world_menu_list_visible_rows;
    }
    if (g_menu_list_selected_index >= g_menu_list_entry_count) {
        g_menu_list_selected_index = g_menu_list_entry_count - 1;
    }
    if (g_world_menu_list_text_table != 0) {
        rect.x = 0x240;
        rect.y = 0x130;
        rect.w = g_world_menu_list_image_width;
        rect.h = g_world_menu_list_visible_rows * 16;
        ClearImage(&rect, 0, 0, 0);
        for (i = 0; i < g_world_menu_list_visible_rows; i++) {
            text_ids[i] = g_world_menu_list_entry_ids[i + g_menu_scroll_base_index];
        }
        text_ids[i] = -1;
        rect.x = 0x240;
        rect.y = 0x130;
        rect.w = g_world_menu_list_image_width;
        rect.h = g_world_menu_row_height;
        world_text_render_id_list_to_image_rows(g_world_menu_list_text_table, text_ids, &rect, 0);
    }
    g_world_menu_list_scroll_direction = 0;
    g_world_menu_list_layout_pending = 0;
}
