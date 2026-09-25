#include "fft/event_bunit.h"

void bunit_menu_run_reorder_list(void) {
    s16 opts[16];
    s16* src;
    s16* dst;
    s32 i;
    s16 result;
    s32 sel;
    s32 keep;

    if (g_bunit_menu_reorder_list_initialized == 0) {
        g_bunit_menu_reorder_list_index = 0;
        bunit_unit_copy_orders_to_reorder_list(1, g_bunit_menu_reorder_list_entries);
        i = 0;
        src = g_bunit_menu_reorder_list_entries;
        dst = opts;
        do {
            *dst = *src + 1;
            src++;
            i++;
            dst++;
        } while (i < 11);
        opts[11] = -1;
        bunit_text_concatenate_ids((s32)g_bunit_text_menu_section, g_bunit_text_reorder_list_buffer, opts, 1);
        g_bunit_menu_reorder_list_initialized = 1;
    }

    if (bunit_menu_run_descriptor_thread(0xF, g_bunit_menu_reorder_list_desc) == 0) {
        g_bunit_menu_reorder_list_initialized = 0;
        g_bunit_menu_current_menu = -1;
    }

    g_bunit_text_selection_id = g_bunit_menu_reorder_list_entries[g_bunit_menu_reorder_list_index] + 0x1002;
    result = g_bunit_menu_reorder_result;
    if (result != -1) {
        sel = result;
        keep = g_bunit_menu_reorder_list_entries[sel];
        for (sel--; sel >= 0; sel--) {
            g_bunit_menu_reorder_list_entries[sel + 1] = g_bunit_menu_reorder_list_entries[sel];
        }
        g_bunit_menu_reorder_list_entries[0] = keep;
        bunit_menu_reset_results();
        i = 0;
        src = g_bunit_menu_reorder_list_entries;
        dst = opts;
        do {
            *dst = *src + 1;
            src++;
            i++;
            dst++;
        } while (i < 11);
        opts[11] = -1;
        bunit_text_concatenate_ids((s32)g_bunit_text_menu_section, g_bunit_text_reorder_list_buffer, opts, 1);
        g_bunit_menu_reorder_list_redraw_request = 1;
        bunit_unit_copy_reorder_list_to_orders(1, (u8*)g_bunit_menu_reorder_list_entries);
        bunit_unit_sort_index_list_by_order_keys(1);
        bunit_unit_build_reorderable_index_list();
        bunit_unit_build_list_index_map();
    }
}
