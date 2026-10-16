/* The item action and sort order menus, units equipping the selected item, and the row window and cursor. */

#include "fft/data.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "fft/world_item.h"
#include "fft/world_menu.h"
#include "psx/pad.h"
#include "psx/types.h"

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
        g_menu_help_message_text_id = 0;
        world_menu_reset_selection_results();
        g_menu_preview_stats_thread_params.style = 0;
        text = menu_text_find_entry_by_index(g_world_formation_menu_text_table, 3, 2);
        if (g_menu_list_entry_count != 0
            && (result = item_adjust_inventory_count(g_item_list_entries[g_menu_list_selected_index], 0)) != 0) {
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
        if (g_menu_list_entry_count != 0
            && item_get_equipped_count_variant(g_item_list_entries[g_menu_list_selected_index]) != 0) {
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
        if (g_item_list_entries[0] != -1 && g_item_list_entries[1] != -1) {
            text[1] = 0;
            g_world_item_action_option_states[2] = -1;
        } else {
            text[1] = 4;
            g_world_item_action_option_states[2] = 0x14D2;
        }
        g_world_menu_text_redraw_request = 1;
        g_world_item_action_menu_initialized = 1;
    }
    result = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_item_action_menu);
    if (result == 0) {
        g_world_item_action_menu_initialized = 0;
        if (g_world_menu_selection_results[3] == -1) {
            return MENU_ITEM_LIST_STATE_BROWSE;
        }
    }
    if (g_world_item_action_option_states[g_item_action_menu.selected_index] >= 0x1000
        && (g_menu_input_primary_repeat & PSX_PAD_CIRCLE)) {
        g_sound_queued_effect_id = MAIN_SFX_INVALID;
    }
    item = g_world_menu_selection_results[3];
    selected = item;
    if (selected != -1) {
        if (selected == 0) {
            g_menu_preview_stats_thread_params.style = 1;
            if (thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) != 0) {
                return MENU_ITEM_LIST_STATE_ACTION;
            }
            if (g_world_menu_selection_results[5] == 0) {
                item_adjust_inventory_count(
                    g_item_list_entries[g_menu_list_selected_index], -g_world_menu_selection_results[4]);
                if (item_get_total_count_variant(g_item_list_entries[g_menu_list_selected_index]) == 0) {
                    for (i = g_menu_list_selected_index; i < g_menu_list_entry_count; i++) {
                        g_item_list_entries[i] = g_item_list_entries[i + 1];
                    }
                    world_menu_init_and_load_scrollable_list(g_item_list_entries, g_menu_list_selected_index,
                        g_text_section_pointers[TEXT_SECTION_ITEM_NAMES], &g_world_item_list_window_script);
                    menu_set_window_scale_step(10);
                    item_rebuild_category_list(g_menu_selected_slot);
                    item_collect_stat_deltas_variant(g_item_list_entries[g_menu_list_selected_index],
                        &g_menu_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
                    g_menu_preview_stats_thread_params.redraw_request = 1;
                }
            }
            g_world_item_action_menu_initialized = 0;
            world_menu_reset_selection_results();
            g_menu_preview_stats_thread_params.style = 0;
            g_world_item_quantity_change_menu.selected_index = 0;
            return MENU_ITEM_LIST_STATE_ACTION;
        } else if (selected == 1) {
            g_menu_preview_stats_thread_params.style = selected;
            item = g_item_list_entries[g_menu_list_selected_index];
            if (g_world_item_action_menu_initialized == selected) {
                g_sound_suppressed = 1;
                if (g_world_menu_selection_results[5] == 0) {
                    g_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
                } else if (g_world_menu_selection_results[5] == 1) {
                    g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                }
                g_world_item_action_menu_initialized = g_sound_queued_effect_id != MAIN_SFX_NONE ? 100 : 1;
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
            g_menu_preview_stats_thread_params.style = 0;
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
        result = menu_update_horizontal_selection_and_mark_change(
            5, WORLD_MENU_CURSOR_ITEM_TAB, g_menu_input_primary_repeat, MAIN_SFX_PAGE_SWITCH);
        if (result == g_menu_selected_slot) {
            return MENU_ITEM_LIST_STATE_ACTION;
        }
        g_world_item_category_input_lock = 1;
        menu_set_selection_record(
            g_menu_selected_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_menu_selected_slot = result;
        world_item_build_category_list(g_unit_selected_index, g_menu_selected_slot + 1,
            g_menu_selected_slot != 4 ? g_menu_selected_slot + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
            ITEM_LIST_MODE_ALL);
        menu_get_selection_record_variant(g_menu_selected_slot, &x, &y, g_item_list_entries);
        menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
        if (g_menu_list_entry_count != 0) {
            result = g_item_list_entries[g_menu_list_selected_index];
        } else {
            result = 0;
        }
        item_collect_stat_deltas_variant(result, &g_menu_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
        g_menu_status_display_thread_params.redraw_request = 1;
        g_menu_preview_stats_thread_params.redraw_request = 1;
    }
    g_world_item_action_menu_initialized = 0;
    return MENU_ITEM_LIST_STATE_ACTION;
}

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
        menu_init_scrollable_list_core_variant(
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

/*
 * Item menu "equipped by" view: browse the formation units that have the
 * highlighted item equipped.
 *
 * The first call rebuilds the formation list filtered by that item and turns
 * on unit browsing and the banner. Cross or Circle (once the comparison
 * banner thread is idle) restores the full list and the selected unit, stops
 * the three panel threads and returns MENU_ITEM_LIST_STATE_BROWSE; until then
 * the result is MENU_ITEM_LIST_STATE_EQUIPPED_BY.
 */
s32 world_item_show_units_equipping_selected(void) {
    if (g_world_item_equipping_units_initialized == 0) {
        world_formation_save_records_to_party();
        g_world_formation_unit_count = world_formation_build_record_list(
            g_item_list_entries[g_menu_list_selected_index], g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
        g_menu_help_message_text_id = MENU_GRID_HELP_UNIT_BROWSE;
        g_world_formation_unit_browse_enabled = 1;
        g_world_formation_unit_banner_enabled = 1;
        world_menu_toggle_preview_stats_window(0);
        g_world_item_saved_selected_unit_index = g_unit_selected_index;
        g_unit_selected_index = 0;
        world_formation_stage_selected_unit();
        g_world_item_equipping_units_initialized = 1;
        menu_set_vsync_mode_and_event_speed(0);
        g_world_formation_unit_cycle_mode = 2;
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0) {
        if ((g_menu_input_newly_pressed & PSX_PAD_CROSS) || (g_menu_input_newly_pressed & PSX_PAD_CIRCLE)) {
            g_world_formation_unit_browse_enabled = 0;
            g_world_formation_unit_banner_enabled = 0;
            g_world_formation_unit_count
                = world_formation_build_record_list(0, g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
            g_unit_selected_index = g_world_item_saved_selected_unit_index;
            world_formation_stage_selected_unit();
            menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
            menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
            menu_request_thread_stop(THREAD_SCREEN_SLOT_UNIT_STATUS);
            g_world_item_equipping_units_initialized = 0;
            g_world_formation_unit_cycle_mode = 0;
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            return MENU_ITEM_LIST_STATE_BROWSE;
        }
    }
    return MENU_ITEM_LIST_STATE_EQUIPPED_BY;
}

/* The incoming mode stays in a2 until the first drawing-helper call. */
void world_menu_draw_row_window_and_cursor(s32 wide, s32 row, s32 mode) {
    s16 x;
    s16 y;
    /* One pointer serves the script then the quad; separate locals remove the argument copy. */
    union {
        menu_instruction_t* script;
        oriented_quad_t* quad;
    } drawing;
    s32 cursor_x;
    oriented_quad_t* cursor;

    x = g_world_item_category_tab_x[row];
    if (wide != 0) {
        x -= 1;
    }
    if (wide == 0) {
        drawing.script = &g_world_item_category_tabs_script;
        y = 0x40;
    } else {
        y = 0x4F;
        drawing.script = &g_world_item_category_tabs_wide_script;
    }
    menu_run_script_with_palette_mode_variant(drawing.script, 0, mode);
    drawing.quad = &g_world_item_category_tab_sprites[row];
    drawing.quad->position.x = x;
    drawing.quad->position.y = y - (row == 0);
    menu_gfx_enqueue_oriented_textured_quad(
        drawing.quad, menu_gfx_get_draw_color_variant(), TEXTURE_UV_FLIP_NONE, 0, 0x2C);
    cursor = &g_world_item_category_tab_cursor_sprite;
    cursor_x = x + 9;
    if (row == 3) {
        cursor_x = x + 7;
    }
    cursor->position.x = cursor_x;
    g_world_item_category_tab_cursor_sprite.position.y = y + 10;
    menu_gfx_enqueue_oriented_textured_quad(cursor, 0, TEXTURE_UV_FLIP_NONE, 0, 0x2D);
}
