/* Item-list menu thread for the formation item screen: builds the item
 * category list on entry, steps the cursor, and refreshes the stat preview.
 */
#include "fft/world.h"
#include "psx/pad.h"

s32 item_run_category_menu_variant(void) {
    s16 x;
    s16 y;
    s32 input;
    s32 selected;
    s32 description;

    if (g_world_item_category_menu_initialized == 0) {
        g_world_item_category_menu_initialized = 1;
        menu_set_vsync_mode_and_event_speed(2);
        g_world_item_category_previewed_cursor = -1;
        world_item_build_category_list(g_unit_selected_index, g_menu_selected_slot + 1,
            g_menu_selected_slot != 4 ? g_menu_selected_slot + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
            ITEM_LIST_MODE_ALL);
        menu_get_selection_record_variant(g_menu_selected_slot, &x, &y, g_item_list_entries);
        menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_world_menu_script_callbacks[0].value = menu_get_entry_item_equipped_count;
        g_world_menu_script_callbacks[1].value = menu_get_entry_item_count;
        g_world_menu_script_callbacks[2].icon = menu_build_item_type_icon_source;
        g_world_menu_script_callbacks[3].icon = menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4].value = menu_get_preview_hp_bonus_display;
        g_world_menu_script_callbacks[5].value = menu_get_preview_mp_bonus_display;
        g_world_menu_script_callbacks[6].value = world_menu_get_selected_unit_hp_bonus;
        g_world_menu_script_callbacks[7].value = world_menu_get_selected_unit_mp_bonus;
    }
    world_menu_toggle_preview_stats_window(1);
    if (g_menu_list_entry_count != 0) {
        description = g_item_list_entries[g_menu_list_selected_index] + 0x6800;
    } else {
        description = -1;
    }
    input = g_menu_input_primary_repeat;
    g_menu_help_message_text_id = description;
    if (input & PSX_PAD_CROSS) {
        menu_set_selection_record(
            g_menu_selected_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_world_item_category_menu_initialized = 0;
        return MENU_ITEM_LIST_STATE_CLOSE;
    }
    selected
        = menu_update_horizontal_selection_and_mark_change(5, WORLD_MENU_CURSOR_ITEM_TAB, input, MAIN_SFX_PAGE_SWITCH);
    if (selected != g_menu_selected_slot) {
        menu_set_selection_record(
            g_menu_selected_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);

        g_menu_selected_slot = selected;
        world_item_build_category_list(g_unit_selected_index, g_menu_selected_slot + 1,
            g_menu_selected_slot != 4 ? g_menu_selected_slot + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
            ITEM_LIST_MODE_ALL);
        g_world_item_category_previewed_cursor = -1;
        menu_get_selection_record_variant(g_menu_selected_slot, &x, &y, g_item_list_entries);
        menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
    } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_menu_list_entry_count != 0) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            menu_set_selection_record(
                selected, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
            return MENU_ITEM_LIST_STATE_REARRANGE;
        }
        g_sound_queued_effect_id = MAIN_SFX_INVALID;
    } else if (g_menu_input_primary_repeat & PSX_PAD_TRIANGLE) {
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        if (item_get_equipped_count_variant(g_item_list_entries[g_menu_list_selected_index]) != 0) {
            g_world_item_category_menu_initialized = 0;
            menu_set_selection_record(
                g_menu_selected_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
            return MENU_ITEM_LIST_STATE_EQUIPPED_BY;
        }
        g_sound_queued_effect_id = MAIN_SFX_INVALID;
    }
    if (g_menu_list_selected_index != g_world_item_category_previewed_cursor) {
        if (g_menu_list_entry_count != 0) {
            selected = g_item_list_entries[g_menu_list_selected_index];
        } else {
            selected = 0;
        }
        g_world_item_category_previewed_cursor = g_menu_list_selected_index;
        item_collect_stat_deltas_variant(selected, &g_menu_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
        g_menu_status_display_thread_params.redraw_request = 1;
        g_menu_preview_stats_thread_params.redraw_request = 1;
    }
    return MENU_ITEM_LIST_STATE_BROWSE;
}
