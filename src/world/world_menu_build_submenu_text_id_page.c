#include "fft/world.h"
#include "psx/types.h"

/* Builds the text-id list for the currently selected sub-menu entry and
 * splits its row count into a visible page (max 7) and an overflow count.
 *
 * A plain for loop fixes the i/dst allocation and one unreferenced local
 * reproduces the 0x20 frame. */
void world_menu_build_submenu_text_id_page(void) {
    s32 selection;
    s32 count;
    s32 i;
    u8* src;
    s32 unused;

    selection = g_world_menu_thread_menu_data[9].selected_index;
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
    world_menu_scrolling_list_thread();
}
