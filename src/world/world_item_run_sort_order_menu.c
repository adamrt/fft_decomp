#include "fft/world.h"
#include "psx/types.h"

/*
 * Run the item-category option strip and return the next item-menu mode:
 * 5 while the strip thread runs, 2 once it closes.
 *
 * On first entry the strip's option ids are loaded and their text built. A
 * chosen option moves to the front of the list, the list is written back and
 * the item list for the category is rebuilt. `i` also carries the description
 * text id; a separate variable changes the target's register allocation.
 */
s8 world_item_run_sort_order_menu(void) {
    s16 text_ids[8];
    s32 i;
    s32 index;
    s32 selected;
    u8 category;

    if (*(s8*)&g_world_item_sort_order_initialized == 0) {
        g_world_item_category_icon_strip_a.cursor = 0;
        g_world_item_category_icon_strip_b.cursor = 0;
        world_menu_widen_byte_record_to_halfwords(g_world_item_menu_category + 1, g_world_item_category_strip_text_ids);
        i = 0;
        while ((text_ids[i] = g_world_item_category_strip_text_ids[i]) != -1) {
            text_ids[i] += 4;
            i++;
        }
        world_text_concatenate_entries(
            g_world_formation_menu_text_table, g_world_formation_menu_text_buffer, text_ids, 1);
        g_world_item_sort_order_initialized = 1;
    }
    if (g_world_item_menu_category == 0 || g_world_item_menu_category == 3) {
        g_world_item_category_icon_strip = &g_world_item_category_icon_strip_a;
    } else {
        g_world_item_category_icon_strip = &g_world_item_category_icon_strip_b;
    }
    i = g_world_item_category_strip_text_ids[g_world_item_category_icon_strip->cursor];
    if (i < 4) {
        i += 0x32;
    }
    if (i == 4) {
        i = 0x37;
    }
    if (i == 5) {
        i = 0x39;
    }
    g_world_menu_description_text_id = i + 0x1000;
    index = g_world_menu_selection_results[7];
    if (index != -1) {
        selected = g_world_item_category_strip_text_ids[index];
        for (i = index - 1; i >= 0; i--) {
            g_world_item_category_strip_text_ids[i + 1] = g_world_item_category_strip_text_ids[i];
        }
        g_world_item_category_strip_text_ids[0] = selected;
        world_menu_reset_selection_results();
        world_menu_narrow_halfwords_to_byte_record(
            g_world_item_menu_category + 1, g_world_item_category_strip_text_ids);
        i = 0;
        while ((text_ids[i] = g_world_item_category_strip_text_ids[i]) != -1) {
            text_ids[i] += 4;
            i++;
        }
        world_text_concatenate_entries(
            g_world_formation_menu_text_table, g_world_formation_menu_text_buffer, text_ids, 1);
        category = g_world_item_menu_category;
        g_world_menu_text_redraw_request = 1;
        /* The definition's u16/s8/u8 parameter conversions would change this call's codegen. */
        ((void (*)(s32, s32, s8, s16*, s32))world_item_build_category_list)(g_world_formation_selected_unit_index,
            category + 0x81, g_world_item_category_view != 4 ? (s8)(category + 1) : 7, g_world_menu_entry_ids, 0);
        world_item_finalize_sorted_list(g_world_item_menu_category, g_world_menu_entry_ids);
        world_menu_init_scrollable_list_core(
            g_world_menu_entry_ids, g_world_menu_cursor_position, g_world_text_item_names);
        world_menu_set_window_scale_step(10);
        world_item_populate_stat_preview(g_world_menu_entry_ids[g_world_menu_cursor_position],
            &g_world_selected_unit_stat_summary, &g_world_item_preview_stat_detail, 0);
        g_world_preview_stats_thread_params.redraw_request = 1;
    }
    if (world_menu_run_thread(6, g_world_item_category_icon_strip) == 0) {
        g_world_item_sort_order_initialized = 0;
        return 2;
    }
    return 5;
}
