/* Equipment item browser and its menu state transitions. */

#include "fft/item_menu.h"
#include "fft/equipment.h"
#include "fft/gfx_image.h"
#include "fft/help.h"
#include "fft/input.h"
#include "fft/item.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/menu_window.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_panel.h"
#include "psx/pad.h"
#include "psx/types.h"

extern s8 g_equipment_ready;
extern s8 g_equipment_unit_banner_enabled;

/* One-shot entry flag for this mode. */
static s8 s_equipment_slot_item_browser_initialized;

/* List index whose stat preview was last built; -1 forces a rebuild. */
extern s8 g_equipment_slot_item_browser_previewed_index;
extern s8 g_equipment_substate; /* menu_item_list_state_e */
extern u8 g_equipment_status_label_image[];
extern RECT g_equipment_status_label_image_rect;

/* Byte-aligned display streams, including the terminating opcode. */
extern u8 g_equipment_render_commands[194];
extern u8 g_equipment_armor_bonus_render_commands[88];

/* Runs one frame of the equipment menu state machine.
 *
 * The first call suspends the command menu thread, stops the status panels and
 * sets up the list menu. Each frame runs the current sub-state handler, which
 * returns the next state; MENU_ITEM_LIST_STATE_CLOSE tears the menu down, restarts
 * the panels, resumes the command menu and returns 0. Otherwise the menu, slot
 * marker and selected-item panels are drawn and 1 is returned.
 */
s32 formation_items_update(void) {
    s32 input;
    s32 override;
    s16 item_id;
    u8 unused[0x18]; /* never accessed; reserves the target's 0x18-byte frame area */

    if (g_equipment_ready == 0) {
        g_equipment_ready = 1;
        g_menu_selected_slot = menu_selection_value_init(2, 0);
        thread_suspend(EQUIPMENT_THREAD_SLOT_COMMAND_MENU);
        menu_request_thread_stop(EQUIPMENT_THREAD_SLOT_UNIT_STATUS);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
        menu_request_thread_stop(EQUIPMENT_THREAD_SLOT_EQUIPMENT_LIST);
        menu_scrollable_list_core_init(0, 0, 0);
        g_equipment_substate = MENU_ITEM_LIST_STATE_BROWSE;
        g_equipment_unit_banner_enabled = 0;
        g_unit_panel_preview_stats_thread_params.position.x = 0;
        g_unit_panel_preview_stats_thread_params.position.y = -0x49;
        gfx_load_image_and_wait(&g_equipment_status_label_image_rect, g_equipment_status_label_image);
    }
    if (g_equipment_substate == MENU_ITEM_LIST_STATE_CLOSE) {
        unit_panel_toggle_item_numbers_thread(0);
        unit_panel_toggle_equipment_threads(1);
        unit_panel_toggle_numbers_thread(1);
        unit_panel_toggle_banner_thread(1);
        g_equipment_ready = 0;
        g_equipment_unit_banner_enabled = 1;
        thread_resume(EQUIPMENT_THREAD_SLOT_COMMAND_MENU);
        g_unit_panel_preview_stats_thread_params.position.x = 2;
        g_unit_panel_preview_stats_thread_params.position.y = 2;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        return 0;
    }
    if (g_equipment_substate == MENU_ITEM_LIST_STATE_BROWSE) {
        g_equipment_substate = formation_items_browse_update();
    } else if (g_equipment_substate == MENU_ITEM_LIST_STATE_REARRANGE) {
        g_equipment_substate = item_rearrange_update();
    } else if (g_equipment_substate == MENU_ITEM_LIST_STATE_ACTION) {
        g_equipment_substate = equipment_update_item_actions();
    } else if (g_equipment_substate == MENU_ITEM_LIST_STATE_SORT_ORDER) {
        g_equipment_substate = equipment_type_order_update();
    }

    /* Only browsing and rearranging take list input and page scrolling. */
    input = 0;
    if (g_equipment_substate < MENU_ITEM_LIST_STATE_ACTION) {
        input = g_menu_input_primary_repeat;
        g_input_page_scroll_disabled = 0;
    } else {
        g_input_page_scroll_disabled = 1;
    }
    override = 0;
    if (g_menu_input_lock_mode != 0 || thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) != 0) {
        override = 1;
    }
    menu_run_script_with_palette_mode(g_equipment_render_commands, input, override);
    equipment_draw_slot_marker(0, g_menu_selected_slot, g_menu_input_lock_mode);
    /* Details of the highlighted item: hand icons for a weapon, the HP/MP bonus for armor
     * and headgear, and its status list. */
    if (g_menu_list_entry_count != 0) {
        item_id = g_item_list_entries[g_menu_list_selected_index];
        if (item_id < ITEM_ID_THROWABLE_FIRST) {
            item_draw_weapon_hand_icons(item_id);
        }
        if (g_item_list_entries[g_menu_list_selected_index] >= ITEM_ID_HEADGEAR_FIRST
            && g_item_list_entries[g_menu_list_selected_index] <= ITEM_ID_ACCESSORY_FIRST - 1) {
            menu_run_script_with_palette_mode(g_equipment_armor_bonus_render_commands, 0, 0);
        }
        item_draw_status_list(g_item_list_entries[g_menu_list_selected_index]);
    }
    return 1;
}

/* Runs the per-slot item browser of the equipment screen.
 *
 * On entry it builds the selected slot's item list and installs the eight row
 * callbacks; each frame it handles cancel, left/right slot changes and
 * confirm, and rebuilds the stat-delta preview when the highlighted row
 * changes. `category` is u16 so its load is a zero_extend:HI that CSE cannot
 * replace with the value just stored to g_menu_selected_slot: the target
 * reloads the slot after the store. */
s32 formation_items_browse_update(void) {
    s16 selected;
    s16 scroll;
    u16 category;
    s32 input;
    s32 slot;

    if (s_equipment_slot_item_browser_initialized == 0) {
        s_equipment_slot_item_browser_initialized = 1;
        category = g_menu_selected_slot;
        g_equipment_slot_item_browser_previewed_index = -1;
        equipment_build_item_list(g_equipment_unit_selected_index, category + 1,
            g_menu_selected_slot != ITEM_MENU_TAB_OTHER ? category + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
            ITEM_LIST_MODE_ALL);
        menu_selection_record_restore(g_menu_selected_slot, &selected, &scroll, g_item_list_entries);
        menu_init_scrollable_list(
            g_item_list_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        g_menu_row_callbacks[0].value = equipment_get_row_equipped_count;
        g_menu_row_callbacks[1].value = equipment_get_row_item_count;
        g_menu_row_callbacks[2].icon = item_build_type_icon_source;
        g_menu_row_callbacks[3].icon = equipment_get_row_icon_source;
        g_menu_row_callbacks[4].value = equipment_get_preview_hp_bonus_display;
        g_menu_row_callbacks[5].value = equipment_get_preview_mp_bonus_display;
        g_menu_row_callbacks[6].value = equipment_get_preview_hp_bonus;
        g_menu_row_callbacks[7].value = equipment_get_preview_mp_bonus;
    }
    unit_panel_toggle_item_numbers_thread(1);
    /* The help text is the item's entry in HELP_TEXT_SECTION_ITEMS. */
    g_help_message_text_id
        = g_menu_list_entry_count != 0 ? g_item_list_entries[g_menu_list_selected_index] + HELP_TEXT_ID_ITEM_BASE : -1;
    input = g_menu_input_primary_repeat;
    if (input & PSX_PAD_CROSS) {
        menu_selection_record_set(
            g_menu_selected_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        s_equipment_slot_item_browser_initialized = 0;
        return MENU_ITEM_LIST_STATE_CLOSE;
    }
    slot = menu_selection_horizontal_sound_step(5, 2, input, SOUND_SFX_PAGE_SWITCH);
    if (slot != g_menu_selected_slot) {
        menu_selection_record_set(
            g_menu_selected_slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        g_menu_selected_slot = slot;
        category = g_menu_selected_slot;
        equipment_build_item_list(g_equipment_unit_selected_index, category + 1,
            g_menu_selected_slot != ITEM_MENU_TAB_OTHER ? category + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries,
            ITEM_LIST_MODE_ALL);
        g_equipment_slot_item_browser_previewed_index = -1;
        menu_selection_record_restore(g_menu_selected_slot, &selected, &scroll, g_item_list_entries);
        menu_init_scrollable_list(
            g_item_list_entries, selected, scroll, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
    } else if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_menu_list_entry_count != 0) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            menu_selection_record_set(slot, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
            return MENU_ITEM_LIST_STATE_REARRANGE;
        }
        g_sound_queued_effect_id = SOUND_SFX_INVALID;
    }
    /* A new highlighted row: preview its item (`slot` now holds the entry). */
    if (g_menu_list_selected_index != g_equipment_slot_item_browser_previewed_index) {
        if (g_menu_list_entry_count != 0) {
            slot = g_item_list_entries[g_menu_list_selected_index];
        } else {
            slot = 0;
        }
        g_equipment_slot_item_browser_previewed_index = g_menu_list_selected_index;
        item_stat_preview_build(slot, &g_unit_panel_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
        g_unit_panel_unit_status_panel_params.redraw_request = 1;
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
    }
    return MENU_ITEM_LIST_STATE_BROWSE;
}
