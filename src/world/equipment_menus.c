/* Equipment updates and the item category menu. */

#include "fft/data.h"
#include "fft/item.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Formation item-menu supervisor: menu 9 of world_formation_item_menu's
 * switch, which calls this as `done = menu_update_equipment_variant()`. Dispatches to the
 * category / rearrange / equip-preview sub-menus and redraws the row window.
 *
 * `busy = 0;` is written after the pressable-button call. The target
 * interleaves `move s0,zero` into that call's argument setup (after
 * `li a2,0x3c`, before `lui a3`); written before the call it is instead
 * emitted first, and reorg then steals it into the preceding `bnez` delay slot
 * and retargets the branch past its own slot instruction. `busy` is not an
 * argument to that call -- it is only read by the script call below -- so the
 * later position is the natural one.
 *
 * fft/world.h owns g_world_formation_unit_cycle_mode,
 * g_world_formation_cursor_ot_override, g_menu_preview_stats_thread_params, g_menu_preview_stats_thread_params.y,
 * g_world_item_submenu_initialized, g_world_menu_* and g_world_formation_unit_banner_enabled, so those are not
 * redeclared here.
 *
 * g_world_item_submenu_initialized is `u8` in the header while the target reads it with `lb`, so it is reached
 * through an lvalue reinterpretation, as item_run_category_menu_variant does for the
 * sibling byte g_world_item_category_menu_initialized (`*(s8*)&g_world_item_category_menu_initialized`).
 *
 * 0x801c8518 carries two cataloged names and the store and the reload use
 * different ones, matching that same twin: one symbol for both lets cse
 * forward the stored value. */
s32 menu_update_equipment_variant(void) {
    s32 input;
    s32 disabled;
    s32 busy;
    s32 unused[6]; /* never referenced; reproduces the 0x48 frame */

    if (g_world_item_submenu_initialized == 0) {
        g_world_item_submenu_initialized = 1;
        g_menu_selected_slot = world_menu_init_cursor_position(WORLD_MENU_CURSOR_ITEM_TAB, 0);
        thread_suspend(WORLD_THREAD_SLOT_UPPER_MENU);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_UNIT_STATUS);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
        menu_request_thread_stop(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL);
        g_world_item_menu_mode = MENU_ITEM_LIST_STATE_BROWSE;
        g_world_formation_unit_banner_enabled = 0;
        g_menu_preview_stats_thread_params.x = 0;
        g_menu_preview_stats_thread_params.y = -0x49;
    }
    g_world_formation_cursor_ot_override = 9;
    if (g_world_item_menu_mode == MENU_ITEM_LIST_STATE_CLOSE) {
        world_menu_toggle_preview_stats_window(0);
        menu_set_vsync_mode_and_event_speed(0);
        g_world_item_submenu_initialized = 0;
        g_world_formation_unit_banner_enabled = 1;
        thread_resume(WORLD_THREAD_SLOT_UPPER_MENU);
        g_menu_preview_stats_thread_params.x = 2;
        g_menu_preview_stats_thread_params.y = 2;
        g_world_formation_unit_cycle_mode = 2;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    if (g_world_item_menu_mode == MENU_ITEM_LIST_STATE_BROWSE) {
        g_world_item_menu_mode = item_run_category_menu_variant();
    } else if (g_world_item_menu_mode == MENU_ITEM_LIST_STATE_REARRANGE) {
        g_world_item_menu_mode = item_run_rearrange_mode();
    } else if (g_world_item_menu_mode == MENU_ITEM_LIST_STATE_ACTION) {
        g_world_formation_cursor_ot_override = 0x3C;
        g_world_item_menu_mode = world_item_run_action_menu();
    } else if (g_world_item_menu_mode == MENU_ITEM_LIST_STATE_SORT_ORDER) {
        g_world_item_menu_mode = world_item_run_sort_order_menu();
    } else if (g_world_item_menu_mode == MENU_ITEM_LIST_STATE_EQUIPPED_BY) {
        g_world_item_menu_mode = world_item_show_units_equipping_selected();
        return 1;
    }
    if (g_world_item_menu_mode == MENU_ITEM_LIST_STATE_EQUIPPED_BY) {
        return 1;
    }
    input = 0;
    if (g_world_item_menu_mode < 2) {
        input = g_menu_input_primary_repeat;
        g_world_menu_prompt_active = 0;
    } else {
        g_world_menu_prompt_active = 1;
    }
    disabled = 0;
    if (world_map_is_busy() != 0 || g_world_item_menu_mode != MENU_ITEM_LIST_STATE_BROWSE
        || item_get_equipped_count_variant(g_item_list_entries[g_menu_list_selected_index]) == 0) {
        disabled = 1;
    }
    world_menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_ITEM_LIST_TRIANGLE, 0xB8, 0x3C,
        g_menu_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, MAIN_SFX_CONFIRM);
    busy = 0;
    if (world_thread_is_task_active(0) != 0 || thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) != 0) {
        busy = 1;
    }
    menu_run_script_with_palette_mode_variant(&g_world_item_list_window_script, input, busy);
    world_menu_draw_row_window_and_cursor(0, g_menu_selected_slot, busy);
    if (g_menu_list_entry_count != 0) {
        if (g_item_list_entries[g_menu_list_selected_index] < ITEM_ID_THROWABLE_FIRST) {
            menu_draw_weapon_hand_icons(g_item_list_entries[g_menu_list_selected_index]);
        }
        if (g_item_list_entries[g_menu_list_selected_index] >= ITEM_ID_HEADGEAR_FIRST
            && g_item_list_entries[g_menu_list_selected_index] < ITEM_ID_ACCESSORY_FIRST) {
            menu_run_script_with_palette_mode_variant(&g_world_item_hp_mp_bonus_window_script, 0, 0);
        }
        menu_draw_item_status_list(g_item_list_entries[g_menu_list_selected_index]);
    }
    return 1;
}

/* Item-list menu thread for the formation item screen: builds the item
 * category list on entry, steps the cursor, and refreshes the stat preview.
 */
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
