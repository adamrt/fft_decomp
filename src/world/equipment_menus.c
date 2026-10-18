/* Equipment updates and the item category menu. */

#include "fft/equipment.h"
#include "fft/formation.h"
#include "fft/formation_item.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/menu_window.h"
#include "fft/screen.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "psx/pad.h"
#include "psx/types.h"

/* item menu state */
extern s8 g_formation_item_menu_mode; /* menu_item_list_state_e */
extern s8 g_formation_item_category_previewed_cursor;

/* Byte-aligned display streams; each extent includes the terminating opcode. */
extern u8 g_formation_item_hp_mp_bonus_window_script[88];

/* Runs one frame of the formation Items screen (menu 9 of formation_item_menu's
 * switch). The first call puts the tab cursor on its first tab, suspends the upper menu and
 * stops the side panels. Each frame then dispatches on g_formation_item_menu_mode to the browse,
 * rearrange, action, sort order or equipped-by sub-menus, and draws the Triangle (equipped-by)
 * button, the list window and the selected item's details. Returns 0 once the screen closes,
 * 1 otherwise.
 *
 * `busy = 0;` is written after the pressable-button call. The target
 * interleaves `move s0,zero` into that call's argument setup (after
 * `li a2,0x3c`, before `lui a3`); written before the call it is instead
 * emitted first, and reorg then steals it into the preceding `bnez` delay slot
 * and retargets the branch past its own slot instruction. `busy` is not an
 * argument to that call -- it is only read by the script call below -- so the
 * later position is the natural one. */
s32 equipment_update_menu(void) {
    s32 input;
    s32 disabled;
    s32 busy;
    s32 unused[6]; /* never referenced; reproduces the 0x48 frame */

    if (g_formation_item_submenu_initialized == 0) {
        g_formation_item_submenu_initialized = 1;
        g_menu_selected_slot = screen_init_cursor_position(SCREEN_CURSOR_ITEM_TAB, 0);
        thread_suspend(SCREEN_THREAD_SLOT_UPPER_MENU);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_UNIT_STATUS);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
        menu_request_thread_stop(SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL);
        g_formation_item_menu_mode = MENU_ITEM_LIST_STATE_BROWSE;
        g_formation_unit_banner_enabled = 0;
        g_unit_panel_preview_stats_thread_params.position.x = 0;
        g_unit_panel_preview_stats_thread_params.position.y = -0x49;
    }
    g_formation_cursor_ot_override = 9;
    if (g_formation_item_menu_mode == MENU_ITEM_LIST_STATE_CLOSE) {
        formation_toggle_preview_stats_window(0);
        menu_set_vsync_mode_and_event_speed(0);
        g_formation_item_submenu_initialized = 0;
        g_formation_unit_banner_enabled = 1;
        thread_resume(SCREEN_THREAD_SLOT_UPPER_MENU);
        g_unit_panel_preview_stats_thread_params.position.x = 2;
        g_unit_panel_preview_stats_thread_params.position.y = 2;
        g_formation_unit_cycle_mode = 2;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        return 0;
    }
    if (g_formation_item_menu_mode == MENU_ITEM_LIST_STATE_BROWSE) {
        g_formation_item_menu_mode = equipment_run_item_list();
    } else if (g_formation_item_menu_mode == MENU_ITEM_LIST_STATE_REARRANGE) {
        g_formation_item_menu_mode = item_run_rearrange_mode();
    } else if (g_formation_item_menu_mode == MENU_ITEM_LIST_STATE_ACTION) {
        g_formation_cursor_ot_override = 0x3C;
        g_formation_item_menu_mode = formation_item_action_menu();
    } else if (g_formation_item_menu_mode == MENU_ITEM_LIST_STATE_SORT_ORDER) {
        g_formation_item_menu_mode = formation_item_sort_menu();
    } else if (g_formation_item_menu_mode == MENU_ITEM_LIST_STATE_EQUIPPED_BY) {
        g_formation_item_menu_mode = formation_item_show_equipping_units();
        return 1;
    }
    if (g_formation_item_menu_mode == MENU_ITEM_LIST_STATE_EQUIPPED_BY) {
        return 1;
    }
    /* Browse and rearrange read the list input; the other states run their own prompts. */
    input = 0;
    if (g_formation_item_menu_mode < MENU_ITEM_LIST_STATE_ACTION) {
        input = g_menu_input_primary_repeat;
        g_screen_prompt_active = 0;
    } else {
        g_screen_prompt_active = 1;
    }
    disabled = 0;
    if (screen_is_busy() != 0 || g_formation_item_menu_mode != MENU_ITEM_LIST_STATE_BROWSE
        || item_get_equipped_count(g_item_list_entries[g_menu_list_selected_index]) == 0) {
        disabled = 1;
    }
    menu_draw_pressable_button(MENU_PRESSABLE_BUTTON_ITEM_LIST_TRIANGLE, 0xB8, 0x3C,
        g_menu_input_primary_repeat & PSX_PAD_TRIANGLE, disabled, SOUND_SFX_CONFIRM);
    busy = 0;
    if (menu_input_get_lock_mode(0) != 0 || thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) != 0) {
        busy = 1;
    }
    menu_run_script_with_palette_mode(g_formation_item_list_window_script, input, busy);
    screen_draw_item_tabs(0, g_menu_selected_slot, busy);
    if (g_menu_list_entry_count != 0) {
        if (g_item_list_entries[g_menu_list_selected_index] < ITEM_ID_THROWABLE_FIRST) {
            item_draw_weapon_hand_icons(g_item_list_entries[g_menu_list_selected_index]);
        }
        if (g_item_list_entries[g_menu_list_selected_index] >= ITEM_ID_HEADGEAR_FIRST
            && g_item_list_entries[g_menu_list_selected_index] < ITEM_ID_ACCESSORY_FIRST) {
            menu_run_script_with_palette_mode(g_formation_item_hp_mp_bonus_window_script, 0, 0);
        }
        item_draw_status_list(g_item_list_entries[g_menu_list_selected_index]);
    }
    return 1;
}

/* The Items screen's browse state: builds the list for the selected tab on
 * entry, and each frame handles the tab switch (rebuilding the list), Circle
 * (rearrange), Triangle (the units equipping the item) and Cross (close),
 * saving the cursor for the tab on the way out. Refreshes the stat preview
 * when the cursor moves. Returns the next item-list state.
 */
s32 equipment_run_item_list(void) {
    s16 selected;
    s16 scroll;
    s32 input;
    s32 slot;
    s32 description;

    if (g_formation_item_category_menu_initialized == 0) {
        g_formation_item_category_menu_initialized = 1;
        menu_set_vsync_mode_and_event_speed(2);
        g_formation_item_category_previewed_cursor = -1;
        formation_item_build_list(g_unit_selected_index, g_menu_selected_slot + 1,
            g_menu_selected_slot != ITEM_MENU_TAB_OTHER ? g_menu_selected_slot + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        menu_get_selection_record(g_menu_selected_slot, &selected, &scroll, g_item_list_entries);
        menu_init_scrollable_list(
            g_item_list_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_menu_row_callbacks[0].value = equipment_get_row_equipped_count;
        g_menu_row_callbacks[1].value = equipment_get_row_item_count;
        g_menu_row_callbacks[2].icon = item_build_type_icon_source;
        g_menu_row_callbacks[3].icon = equipment_get_row_icon_source;
        g_menu_row_callbacks[4].value = equipment_get_preview_hp_bonus_display;
        g_menu_row_callbacks[5].value = equipment_get_preview_mp_bonus_display;
        g_menu_row_callbacks[6].value = formation_item_get_hp_bonus;
        g_menu_row_callbacks[7].value = formation_item_get_mp_bonus;
    }
    formation_toggle_preview_stats_window(1);
    if (g_menu_list_entry_count != 0) {
        description = g_item_list_entries[g_menu_list_selected_index] + HELP_TEXT_ID_ITEM_BASE;
    } else {
        description = -1;
    }
    input = g_menu_input_primary_repeat;
    g_help_message_text_id = description;
    if (input & PSX_PAD_CROSS) {
        menu_set_selection_record(
            g_menu_selected_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_formation_item_category_menu_initialized = 0;
        return MENU_ITEM_LIST_STATE_CLOSE;
    }
    slot = menu_update_horizontal_selection_and_mark_change(5, SCREEN_CURSOR_ITEM_TAB, input, SOUND_SFX_PAGE_SWITCH);
    if (slot != g_menu_selected_slot) {
        menu_set_selection_record(
            g_menu_selected_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);

        g_menu_selected_slot = slot;
        formation_item_build_list(g_unit_selected_index, g_menu_selected_slot + 1,
            g_menu_selected_slot != ITEM_MENU_TAB_OTHER ? g_menu_selected_slot + 1 : ITEM_LIST_FILTER_OTHER,
            g_item_list_entries, ITEM_LIST_MODE_ALL);
        g_formation_item_category_previewed_cursor = -1;
        menu_get_selection_record(g_menu_selected_slot, &selected, &scroll, g_item_list_entries);
        menu_init_scrollable_list(
            g_item_list_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
    } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_menu_list_entry_count != 0) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            menu_set_selection_record(slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
            return MENU_ITEM_LIST_STATE_REARRANGE;
        }
        g_sound_queued_effect_id = SOUND_SFX_INVALID;
    } else if (g_menu_input_primary_repeat & PSX_PAD_TRIANGLE) {
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        if (item_get_equipped_count(g_item_list_entries[g_menu_list_selected_index]) != 0) {
            g_formation_item_category_menu_initialized = 0;
            menu_set_selection_record(
                g_menu_selected_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
            return MENU_ITEM_LIST_STATE_EQUIPPED_BY;
        }
        g_sound_queued_effect_id = SOUND_SFX_INVALID;
    }
    if (g_menu_list_selected_index != g_formation_item_category_previewed_cursor) {
        if (g_menu_list_entry_count != 0) {
            slot = g_item_list_entries[g_menu_list_selected_index];
        } else {
            slot = 0;
        }
        g_formation_item_category_previewed_cursor = g_menu_list_selected_index;
        item_collect_stat_deltas(slot, &g_unit_panel_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
    }
    return MENU_ITEM_LIST_STATE_BROWSE;
}
