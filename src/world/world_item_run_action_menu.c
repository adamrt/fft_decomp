#include "fft/world.h"
#include "psx/pad.h"

/*
 * Run the formation item-menu action strip (menu mode 2) and return the next
 * item-menu mode: 5 while the strip stays open, 2 once an action completes.
 *
 * On entry the three option lines are enabled or greyed from the selected
 * item. Option 0 changes the owned quantity, option 1 unequips the item from
 * every formation unit and -1 steps the item category.
 *
 * `item` first holds the chosen option and `selected` copies it: the target
 * compares g_world_item_action_menu_initialized against that copy after `item` is reused for the item
 * id. The zero-instruction barrier reproduces the reload of the category byte
 * after the category-view store and the early `4` compare constant, exactly
 * as in src/world/world_item_run_category_menu.c.
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
    u8 category;

    if (*(s8*)&g_world_item_action_menu_initialized == 0) {
        g_world_item_category_input_lock = 0;
        g_world_menu_description_text_id = 0;
        world_menu_reset_selection_results();
        g_world_preview_stats_thread_params.style = 0;
        text = world_text_find_entry_by_index(g_world_formation_menu_text_table, 3, 2);
        if (g_world_menu_option_count != 0
            && (result = world_item_change_quantity_on_equip(g_world_menu_entry_ids[g_world_menu_cursor_position], 0))
                != 0) {
            text[1] = 0;
            g_world_item_action_option_states[0] = 1;
            g_world_item_action_quantity_range.max = result;
        } else {
            text[1] = 4;
            g_world_item_action_option_states[0] = 0x14D2;
        }
        text++;
        while (*text != 0xE3) {
            text++;
        }
        if (g_world_menu_option_count != 0
            && world_count_item_equipped_by_party(g_world_menu_entry_ids[g_world_menu_cursor_position]) != 0) {
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
    result = world_menu_run_thread(6, &g_world_item_action_menu);
    if (result == 0) {
        g_world_item_action_menu_initialized = 0;
        if (g_world_menu_selection_results[3] == -1) {
            return 0;
        }
    }
    if (g_world_item_action_option_states[g_world_item_action_menu.cursor] >= 0x1000
        && (g_world_input_primary_repeat & PSX_PAD_CIRCLE)) {
        g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
    }
    item = g_world_menu_selection_results[3];
    selected = item;
    if (selected != -1) {
        if (selected == 0) {
            g_world_preview_stats_thread_params.style = 1;
            if (world_thread_is_running(5) != 0) {
                return 2;
            }
            if (g_world_menu_selection_results[5] == 0) {
                world_item_change_quantity_on_equip(
                    g_world_menu_entry_ids[g_world_menu_cursor_position], -g_world_menu_selection_results[4]);
                if (world_item_count_owned_and_equipped(g_world_menu_entry_ids[g_world_menu_cursor_position]) == 0) {
                    for (i = g_world_menu_cursor_position; i < g_world_menu_option_count; i++) {
                        g_world_menu_entry_ids[i] = g_world_menu_entry_ids[i + 1];
                    }
                    world_menu_init_and_load_scrollable_list(g_world_menu_entry_ids, g_world_menu_cursor_position,
                        g_world_text_item_names, g_world_item_list_window_script);
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
            g_world_item_quantity_change_menu.cursor = 0;
            return 2;
        } else if (selected == 1) {
            g_world_preview_stats_thread_params.style = selected;
            item = g_world_menu_entry_ids[g_world_menu_cursor_position];
            if (*(s8*)&g_world_item_action_menu_initialized == selected) {
                g_world_menu_sound_muted = 1;
                if (g_world_menu_selection_results[5] == 0) {
                    g_world_menu_sound_effect_id = MAIN_SFX_UNEQUIP;
                } else if (g_world_menu_selection_results[5] == 1) {
                    g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
                }
                g_world_item_action_menu_initialized = g_world_menu_sound_effect_id != 0 ? 100 : 1;
            }
            if (world_thread_is_running(5) != 0) {
                return 2;
            }
            if (g_world_menu_selection_results[5] == 0) {
                for (i = 0; i < g_world_formation_unit_count; i++) {
                    for (slot = 0; slot < 5; slot++) {
                        if (item == g_world_formation_unit_pointers[i]->equipment[slot]) {
                            world_formation_equip_item_to_unit_slot(i, slot, 0);
                        }
                    }
                }
            }
            g_world_menu_sound_muted = 0;
            world_menu_reset_selection_results();
            g_world_preview_stats_thread_params.style = 0;
            g_world_item_unequip_all_menu.cursor = 0;
        } else {
            if (result == 0) {
                return 5;
            }
            return 2;
        }
    } else {
        if (world_thread_is_running(5) != 0) {
            return 2;
        }
        result = world_menu_step_cursor_with_sound_2(5, 7, g_world_input_primary_repeat, 6);
        if (result == g_world_item_menu_category) {
            return 2;
        }
        g_world_item_category_input_lock = 1;
        world_gfx_set_sprite_slot(g_world_item_menu_category, g_world_menu_cursor_position, g_world_menu_scroll_offset,
            (u16*)g_world_menu_entry_ids);
        {
            s32 four = 4;

            g_world_item_category_view = result;
            /* The view byte aliases g_world_item_menu_category: keeps its reloads
             * after this store, with the 4 materialised first. */
            __asm__ volatile("" : : "r"(four) : "memory");
            category = g_world_item_menu_category;
            /* The definition's u16/s8/u8 parameter conversions would change this call's codegen. */
            ((void (*)(s32, s32, s8, s16*, s32))world_item_build_category_list)(g_world_formation_selected_unit_index,
                category + 1, g_world_item_menu_category != four ? (s8)(category + 1) : 7, g_world_menu_entry_ids, 0);
        }
        world_gfx_get_sprite_slot(g_world_item_menu_category, &x, &y, g_world_menu_entry_ids);
        world_menu_init_scrollable_list(g_world_menu_entry_ids, x, y, g_world_text_item_names);
        world_menu_set_window_scale_step(10);
        if (g_world_menu_option_count != 0) {
            result = g_world_menu_entry_ids[g_world_menu_cursor_position];
        } else {
            result = 0;
        }
        world_item_populate_stat_preview(
            (s16)result, &g_world_selected_unit_stat_summary, &g_world_item_preview_stat_detail, 0);
        g_world_formation_panel_windows[0].enabled = 1;
        g_world_preview_stats_thread_params.redraw_request = 1;
    }
    g_world_item_action_menu_initialized = 0;
    return 2;
}
