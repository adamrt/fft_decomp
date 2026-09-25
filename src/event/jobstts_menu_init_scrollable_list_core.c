#include "fft/event_jobstts.h"
#include "psx/types.h"

void jobstts_menu_init_scrollable_list_core(s16* entries, s32 selected_index, s32 data) {
    g_jobstts_menu_list_entries = (u16*)entries;
    g_jobstts_menu_list_text_table = (u16*)data;
    if (entries != 0) {
        s32 first = *entries;
        g_jobstts_menu_list_entry_count = 0;
        if (first != -1) {
            do {
                g_jobstts_menu_list_entry_count = g_jobstts_menu_list_entry_count + 1;
            } while (entries[g_jobstts_menu_list_entry_count] != -1);
        }
    }
    g_jobstts_menu_list_selected_index = selected_index;
    if (selected_index == 0) {
        g_jobstts_menu_scroll_base_index = 0;
    }
    g_jobstts_text_color[0] = 0x80;
    g_jobstts_text_color[1] = 0x80;
    g_jobstts_text_color[2] = 0x80;
    g_jobstts_gfx_otag_index = 0;
    g_jobstts_menu_list_row_rendering = 0;
    g_jobstts_menu_list_scroll_offset_y = 0;
    g_jobstts_menu_list_redraw_pending = 1;
    g_jobstts_gfx_transition_frame = 0;
}
