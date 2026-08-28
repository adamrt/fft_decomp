#include "fft/world.h"

void world_menu_run_order_unit(void) {
    s16 option_ids[12]; /* seven entries plus terminator; sized for the retail frame */
    s32 index;
    s32 selected_entry;

    if (g_world_menu_order_unit_open == 0) {
        world_menu_widen_byte_record_to_halfwords(0, g_world_menu_order_unit_entries);
        for (index = 0; index < 7; index++) {
            option_ids[index] = g_world_menu_order_unit_entries[index] + 14;
        }
        option_ids[7] = -1;
        world_text_concatenate_entries(
            g_world_formation_menu_text_table, g_world_formation_menu_text_buffer, option_ids, 1);
        g_world_formation_unit_banner_enabled = 0;
        g_world_order_unit_saved_browse_enabled = g_world_formation_unit_browse_enabled;
        g_world_formation_unit_browse_enabled = 0;
        world_menu_stop_unit_status_banner_thread(9);
        world_menu_stop_unit_status_banner_thread(12);
        g_world_menu_order_unit_cursor = 0;
        g_world_menu_order_unit_open = 1;
    }

    if (world_menu_run_thread(15, g_world_order_unit_menu) == 0) {
        g_world_menu_order_unit_open = 0;
        g_world_formation_current_menu = 0;
        g_world_formation_unit_browse_enabled = g_world_order_unit_saved_browse_enabled;
    }

    g_world_menu_description_text_id = g_world_menu_order_unit_entries[g_world_menu_order_unit_cursor] + 0x103f;
    if (g_world_menu_selection_results[9] != -1) {
        /* Move the chosen entry to the front and redraw the options. */
        index = g_world_menu_selection_results[9];
        selected_entry = g_world_menu_order_unit_entries[index];
        index -= 1;
        for (; index >= 0; index--) {
            g_world_menu_order_unit_entries[index + 1] = g_world_menu_order_unit_entries[index];
        }
        g_world_menu_order_unit_entries[0] = selected_entry;
        world_menu_reset_selection_results();
        for (index = 0; index < 7; index++) {
            option_ids[index] = g_world_menu_order_unit_entries[index] + 14;
        }
        option_ids[7] = -1;
        world_text_concatenate_entries(
            g_world_formation_menu_text_table, g_world_formation_menu_text_buffer, option_ids, 1);
        g_world_menu_text_redraw_request = 1;
        world_menu_narrow_halfwords_to_byte_record(0, g_world_menu_order_unit_entries);
        world_formation_sort_roster(0);
    }
}
