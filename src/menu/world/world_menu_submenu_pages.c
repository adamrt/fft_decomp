/* Sub-menu text-id page, slot 4 cursor preset and a noop. */

#include "fft/world.h"
#include "psx/types.h"

void world_menu_build_submenu_text_id_page(void) {
    s32 selection;
    s32 count;
    s32 i;
    u8* src;

    selection = g_menu_thread_menu_data[9].selected_index;
    count = g_world_menu_submenu_page_counts[selection];
    src = g_world_menu_submenu_page_text_ids[selection];
    for (i = 0; i < count; i++) {
        g_world_menu_submenu_page_row_text_ids[i] = src[i] + 0x902B;
    }
    if (count < 7) {
        g_world_menu_submenu_page_layout.columns.row_count = count;
        g_world_menu_submenu_page_layout.columns.hidden_rows = 0;
    } else {
        g_world_menu_submenu_page_layout.columns.row_count = 7;
        g_world_menu_submenu_page_layout.columns.hidden_rows = count - 7;
    }
    menu_run_scrolling_ability_list_thread_variant();
}

/* Preset the selected row of default menu entry 4. */
void world_menu_set_slot_4_cursor_index(s32 index) {
    g_world_menu_default_entries[4].selected_index = index;
}

void world_noop_80106138(void) {
}
