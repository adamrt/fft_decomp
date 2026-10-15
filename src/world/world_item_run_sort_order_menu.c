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

    if (g_world_item_sort_order_initialized == 0) {
        g_world_item_category_icon_strip_a.selected_index = 0;
        g_world_item_category_icon_strip_b.selected_index = 0;
        menu_load_sort_key_list(g_menu_selected_slot + 1, g_world_item_category_strip_text_ids);
        i = 0;
        while ((text_ids[i] = g_world_item_category_strip_text_ids[i]) != -1) {
            text_ids[i] += 4;
            i++;
        }
        text_concatenate_entries(g_world_formation_menu_text_table, g_world_formation_menu_text_buffer, text_ids, 1);
        g_world_item_sort_order_initialized = 1;
    }
    if (g_menu_selected_slot == 0 || g_menu_selected_slot == 3) {
        g_world_item_category_icon_strip = &g_world_item_category_icon_strip_a;
    } else {
        g_world_item_category_icon_strip = &g_world_item_category_icon_strip_b;
    }
    i = g_world_item_category_strip_text_ids[g_world_item_category_icon_strip->selected_index];
    if (i < 4) {
        i += 0x32;
    }
    if (i == 4) {
        i = 0x37;
    }
    if (i == 5) {
        i = 0x39;
    }
    g_menu_help_message_text_id = i + 0x1000;
    index = g_world_menu_selection_results[7];
    if (index != -1) {
        selected = g_world_item_category_strip_text_ids[index];
        for (i = index - 1; i >= 0; i--) {
            g_world_item_category_strip_text_ids[i + 1] = g_world_item_category_strip_text_ids[i];
        }
        g_world_item_category_strip_text_ids[0] = selected;
        world_menu_reset_selection_results();
        menu_store_sort_key_list(g_menu_selected_slot + 1, g_world_item_category_strip_text_ids);
        i = 0;
        while ((text_ids[i] = g_world_item_category_strip_text_ids[i]) != -1) {
            text_ids[i] += 4;
            i++;
        }
        text_concatenate_entries(g_world_formation_menu_text_table, g_world_formation_menu_text_buffer, text_ids, 1);
        g_world_menu_text_redraw_request = 1;
        world_item_build_category_list(g_unit_selected_index, g_menu_selected_slot + 0x81,
            g_menu_selected_slot != 4 ? g_menu_selected_slot + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
            ITEM_LIST_MODE_ALL);
        item_store_category_list(g_menu_selected_slot, g_item_list_entries);
        world_menu_init_scrollable_list_core(
            g_item_list_entries, g_menu_list_selected_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
        item_collect_stat_deltas_variant(g_item_list_entries[g_menu_list_selected_index],
            &g_menu_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
        g_menu_preview_stats_thread_params.redraw_request = 1;
    }
    if (menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_world_item_category_icon_strip) == 0) {
        g_world_item_sort_order_initialized = 0;
        return MENU_ITEM_LIST_STATE_ACTION;
    }
    return MENU_ITEM_LIST_STATE_SORT_ORDER;
}
