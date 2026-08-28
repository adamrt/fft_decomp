#include "fft/bunit.h"

void bunit_menu_init_scrollable_list_core(s16* entries, s32 selected_index, s32 text_table) {
    s32 first;

    g_bunit_menu_list_entries = (u16*)entries;
    g_bunit_menu_list_text_table = (u16*)text_table;
    if (entries != 0) {
        first = entries[0];
        g_bunit_menu_list_entry_count = 0;
        if (first != -1) {
            do {
                g_bunit_menu_list_entry_count = g_bunit_menu_list_entry_count + 1;
            } while (entries[(s16)g_bunit_menu_list_entry_count] != -1);
        }
    }
    g_bunit_menu_list_selected_index = selected_index;
    if (selected_index == 0) {
        g_bunit_menu_scroll_base_index = 0;
    }
    g_bunit_gfx_sprite_color[0] = 0x80;
    g_bunit_gfx_sprite_color[1] = 0x80;
    g_bunit_gfx_sprite_color[2] = 0x80;
    g_bunit_gfx_otag_index = 0;
    g_bunit_menu_scroll_list_active = 0;
    g_bunit_menu_scroll_pixel_offset = 0;
    g_bunit_menu_list_redraw_pending = 1;
    g_bunit_gfx_transition_frame = 0;
    g_bunit_gfx_otag_index_locked = 0;
    g_bunit_input_page_scroll_disabled = 0;
}
