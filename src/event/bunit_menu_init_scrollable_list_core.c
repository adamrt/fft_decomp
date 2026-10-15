#include "fft/event_bunit.h"
#include "fft/menu.h"
#include "fft/menu_script.h"

/* Point the scrollable list menu at a new -1 terminated entry list and reset
 * its render state. The scroll base is reset only when the selected index
 * is 0. */
void bunit_menu_init_scrollable_list_core(s16* entries, s32 selected_index, u8* text_table) {
    s32 first;

    g_bunit_menu_list_entries = entries;
    g_bunit_menu_list_text_table = text_table;
    if (entries != 0) {
        first = entries[0];
        g_bunit_menu_list_entry_count = 0;
        if (first != -1) {
            do {
                g_bunit_menu_list_entry_count = g_bunit_menu_list_entry_count + 1;
            } while (entries[g_bunit_menu_list_entry_count] != -1);
        }
    }
    g_bunit_menu_list_selected_index = selected_index;
    if (selected_index == 0) {
        g_menu_scroll_base_index = 0;
    }
    g_menu_text_color.r = 0x80;
    g_menu_text_color.g = 0x80;
    g_menu_text_color.b = 0x80;
    g_menu_gfx_otag_index = 0;
    g_menu_list_row_mode = 0;
    g_menu_list_scroll_offset = 0;
    g_bunit_menu_list_redraw_pending = 1;
    g_gfx_transition_frame = 0;
    g_gfx_otag_index_locked = 0;
    g_bunit_input_page_scroll_disabled = 0;
}
