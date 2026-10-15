/* Scrollable list layout load, init and the WORLD init-and-draw variant. */

#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

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

/* Initialize the list state, then load its visible entries from the menu
 * definition. */
void world_menu_init_and_load_scrollable_list(
    const s16* values, s32 selected_index, u8* text_table, menu_instruction_t* script) {
    menu_init_scrollable_list_core_variant(values, selected_index, text_table);
    world_menu_load_scrollable_list_layout(script);
}

/* Built at -O1: the unscheduled prologue (sw ra, sw s0, lw s0) is the -O1 signature shared by the
 * neighboring menu-script handlers. */

void menu_init_and_draw_scrollable_list_variant(
    const s16* values, s32 value, s32 selected, u8* text_table, const menu_instruction_t* script) {
    menu_init_scrollable_list(values, value, selected, text_table);
    world_menu_load_scrollable_list_layout(script);
}
