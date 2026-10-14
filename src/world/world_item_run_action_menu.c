#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"

/*
 * Run the formation item-menu action strip (menu mode 2) and return the next
 * item-menu mode: 5 while the strip stays open, 2 once an action completes.
 *
 * On entry the three option lines are enabled or grayed from the selected
 * item. Option 0 changes the owned quantity, option 1 unequips the item from
 * every formation unit and -1 steps the item category.
 *
 * `item` first holds the chosen option and `selected` copies it: the target
 * compares g_world_item_action_menu_initialized against that copy after `item` is reused for the item
 * id.
 */
s8 world_item_run_action_menu(void) {
    s16 x;
    s16 y;
    u8* text;
    s32 result;
    s32 selected;
    s32 item;
    s32 i;
    s32 slot;

    if (g_world_item_action_menu_initialized == 0) {
        g_world_item_category_input_lock = 0;
        g_world_menu_description_text_id = 0;
        world_menu_reset_selection_results();
        g_world_preview_stats_thread_params.style = 0;
        text = world_text_find_entry_by_index(g_world_formation_menu_text_table, 3, 2);
        if (g_world_menu_option_count != 0
            && (result = world_item_adjust_inventory_count(g_world_menu_entry_ids[g_world_menu_cursor_position], 0))
                != 0) {
            text[1] = 0;
            g_world_item_action_option_states[0] = 1;
            g_world_item_action_quantity_range.bounds.max = result;
        } else {
            text[1] = 4;
            g_world_item_action_option_states[0] = 0x14D2;
        }
        text++;
        while (*text != 0xE3) {
            text++;
        }
        if (g_world_menu_option_count != 0
            && world_item_count_equipped_by_party(g_world_menu_entry_ids[g_world_menu_cursor_position]) != 0) {
            text[1] = 0;
            g_world_item_action_option_states[1] = 4;
        } else {
            text[1] = 4;
            g_world_item_action_option_states[1] = 0x14D2;
        }
        text++;
        while (*text != 0xE3) {
            text++;
        }
        if (g_world_menu_entry_ids[0] != -1 && g_world_menu_entry_ids[1] != -1) {
            text[1] = 0;
            g_world_item_action_option_states[2] = -1;
        } else {
            text[1] = 4;
            g_world_item_action_option_states[2] = 0x14D2;
        }
        g_world_menu_text_redraw_request = 1;
        g_world_item_action_menu_initialized = 1;
    }
    result = world_menu_run_thread(THREAD_SCREEN_SLOT_MENU, &g_world_item_action_menu);
    if (result == 0) {
        g_world_item_action_menu_initialized = 0;
        if (g_world_menu_selection_results[3] == -1) {
            return MENU_ITEM_LIST_STATE_BROWSE;
        }
    }
    if (g_world_item_action_option_states[g_world_item_action_menu.selected_index] >= 0x1000
        && (g_world_input_primary_repeat & PSX_PAD_CIRCLE)) {
        g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
    }
    item = g_world_menu_selection_results[3];
    selected = item;
    if (selected != -1) {
        if (selected == 0) {
            g_world_preview_stats_thread_params.style = 1;
            if (thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) != 0) {
                return MENU_ITEM_LIST_STATE_ACTION;
            }
            if (g_world_menu_selection_results[5] == 0) {
                world_item_adjust_inventory_count(
                    g_world_menu_entry_ids[g_world_menu_cursor_position], -g_world_menu_selection_results[4]);
                if (world_item_count_owned_and_equipped(g_world_menu_entry_ids[g_world_menu_cursor_position]) == 0) {
                    for (i = g_world_menu_cursor_position; i < g_world_menu_option_count; i++) {
                        g_world_menu_entry_ids[i] = g_world_menu_entry_ids[i + 1];
                    }
                    world_menu_init_and_load_scrollable_list(g_world_menu_entry_ids, g_world_menu_cursor_position,
                        g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_world_item_list_window_script);
                    world_menu_set_window_scale_step(10);
                    world_item_reconcile_sorted_list(g_world_item_menu_category);
                    world_item_populate_stat_preview(g_world_menu_entry_ids[g_world_menu_cursor_position],
                        &g_world_selected_unit_stat_summary, &g_world_item_preview_stat_detail, 0);
                    g_world_preview_stats_thread_params.redraw_request = 1;
                }
            }
            g_world_item_action_menu_initialized = 0;
            world_menu_reset_selection_results();
            g_world_preview_stats_thread_params.style = 0;
            g_world_item_quantity_change_menu.selected_index = 0;
            return MENU_ITEM_LIST_STATE_ACTION;
        } else if (selected == 1) {
            g_world_preview_stats_thread_params.style = selected;
            item = g_world_menu_entry_ids[g_world_menu_cursor_position];
            if (g_world_item_action_menu_initialized == selected) {
                g_sound_suppressed = 1;
                if (g_world_menu_selection_results[5] == 0) {
                    g_world_menu_sound_effect_id = MAIN_SFX_UNEQUIP;
                } else if (g_world_menu_selection_results[5] == 1) {
                    g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
                }
                g_world_item_action_menu_initialized = g_world_menu_sound_effect_id != MAIN_SFX_NONE ? 100 : 1;
            }
            if (thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) != 0) {
                return MENU_ITEM_LIST_STATE_ACTION;
            }
            if (g_world_menu_selection_results[5] == 0) {
                for (i = 0; i < g_world_formation_unit_count; i++) {
                    for (slot = 0; slot < WORLD_EQUIPMENT_SLOT_COUNT; slot++) {
                        if (item == g_world_unit_pointers[i]->equipment[slot]) {
                            world_formation_equip_item_to_unit_slot(i, slot, ITEM_ID_NOTHING);
                        }
                    }
                }
            }
            g_sound_suppressed = 0;
            world_menu_reset_selection_results();
            g_world_preview_stats_thread_params.style = 0;
            g_world_item_unequip_all_menu.selected_index = 0;
        } else {
            if (result == 0) {
                return MENU_ITEM_LIST_STATE_SORT_ORDER;
            }
            return MENU_ITEM_LIST_STATE_ACTION;
        }
    } else {
        if (thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) != 0) {
            return MENU_ITEM_LIST_STATE_ACTION;
        }
        result = world_menu_update_horizontal_selection_and_mark_change(
            5, WORLD_MENU_CURSOR_ITEM_TAB, g_world_input_primary_repeat, MAIN_SFX_PAGE_SWITCH);
        if (result == g_world_item_menu_category) {
            return MENU_ITEM_LIST_STATE_ACTION;
        }
        g_world_item_category_input_lock = 1;
        world_menu_set_selection_record(g_world_item_menu_category, g_world_menu_cursor_position,
            g_world_menu_scroll_offset, g_world_menu_entry_ids);
        g_world_item_menu_category = result;
        world_item_build_category_list(g_world_formation_selected_unit_index, g_world_item_menu_category + 1,
            g_world_item_menu_category != 4 ? g_world_item_menu_category + 1 : ITEM_LIST_FILTER_OTHER,
            g_world_menu_entry_ids, ITEM_LIST_MODE_ALL);
        world_menu_get_selection_record(g_world_item_menu_category, &x, &y, g_world_menu_entry_ids);
        world_menu_init_scrollable_list(g_world_menu_entry_ids, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        world_menu_set_window_scale_step(10);
        if (g_world_menu_option_count != 0) {
            result = g_world_menu_entry_ids[g_world_menu_cursor_position];
        } else {
            result = 0;
        }
        world_item_populate_stat_preview(
            result, &g_world_selected_unit_stat_summary, &g_world_item_preview_stat_detail, 0);
        g_world_status_display_thread_params.redraw_request = 1;
        g_world_preview_stats_thread_params.redraw_request = 1;
    }
    g_world_item_action_menu_initialized = 0;
    return MENU_ITEM_LIST_STATE_ACTION;
}
