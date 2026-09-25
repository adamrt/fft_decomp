#include "fft/event_equip.h"
#include "psx/types.h"

/* Point the scrollable list menu at a new -1 terminated entry list and reset
   its render state. */
void equip_menu_init_scrollable_list_core(s16* entries, s32 selected_index, const void* text_table) {
    g_equip_menu_list_entries = (u16*)entries;
    g_equip_menu_list_text_table = (u16*)text_table;

    if (entries != 0) {
        g_equip_menu_list_entry_count = 0;
        while (entries[g_equip_menu_list_entry_count] != -1) {
            g_equip_menu_list_entry_count = g_equip_menu_list_entry_count + 1;
        }
    }

    g_equip_menu_selected_list_index = selected_index;
    if (selected_index == 0) {
        g_equip_menu_scroll_base_index = 0;
    }

    g_equip_gfx_sprite_color[0] = 0x80;
    g_equip_gfx_sprite_color[1] = 0x80;
    g_equip_gfx_sprite_color[2] = 0x80;
    g_equip_gfx_sprite_ot_index = 0;
    g_equip_menu_list_row_mode = 0;
    g_equip_menu_list_scroll_offset = 0;
    g_equip_menu_list_redraw_pending = 1;
    g_equip_gfx_transition_frame = 0;
    g_equip_gfx_zoom_draw_area_active = 0;
}
