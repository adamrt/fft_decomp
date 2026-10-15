#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Runs one frame of the equipment menu state machine.
 *
 * The first call suspends thread 5 and sets up the list menu. Each frame runs
 * the current sub-state handler, which returns the next state; state -1 tears
 * the menu down, resumes thread 5 and returns 0. Otherwise the menu, slot
 * marker and selected-item panels are drawn and 1 is returned.
 */
s32 equip_menu_update_equipment(void) {
    s32 input;
    s32 override;
    s16 item_id;
    u8 unused[0x18]; /* never accessed; reserves the target's 0x18-byte frame area */

    if (g_equip_menu_ready == 0) {
        g_equip_menu_ready = 1;
        g_equip_selected_slot = equip_menu_init_selection_value(2, 0);
        thread_suspend(EQUIP_THREAD_SLOT_COMMAND_MENU);
        menu_request_thread_stop(EQUIP_THREAD_SLOT_UNIT_STATUS);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
        menu_request_thread_stop(EQUIP_THREAD_SLOT_EQUIPMENT_LIST);
        equip_menu_init_scrollable_list_core(0, 0, 0);
        g_equip_menu_substate = MENU_ITEM_LIST_STATE_BROWSE;
        g_equip_unit_banner_enabled = 0;
        g_equip_item_numeric_panel_params = 0;
        g_equip_item_numeric_panel_y = -0x49;
        gfx_load_image_and_wait(&g_equip_status_label_image_rect, g_equip_status_label_image);
    }
    if (g_equip_menu_substate == MENU_ITEM_LIST_STATE_CLOSE) {
        equip_panel_toggle_item_numeric_thread(0);
        equip_panel_toggle_equipment_threads(1);
        equip_panel_toggle_numeric_editor_thread(1);
        equip_panel_toggle_unit_status_thread(1);
        g_equip_menu_ready = 0;
        g_equip_unit_banner_enabled = 1;
        thread_resume(EQUIP_THREAD_SLOT_COMMAND_MENU);
        g_equip_item_numeric_panel_params = 2;
        g_equip_item_numeric_panel_y = 2;
        g_equip_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    if (g_equip_menu_substate == MENU_ITEM_LIST_STATE_BROWSE) {
        g_equip_menu_substate = equip_menu_run_slot_item_browser();
    } else if (g_equip_menu_substate == MENU_ITEM_LIST_STATE_REARRANGE) {
        g_equip_menu_substate = equip_item_run_rearrange_mode();
    } else if (g_equip_menu_substate == MENU_ITEM_LIST_STATE_ACTION) {
        g_equip_menu_substate = equip_menu_update_item_action_state();
    } else if (g_equip_menu_substate == MENU_ITEM_LIST_STATE_SORT_ORDER) {
        g_equip_menu_substate = equip_item_run_type_order_mode();
    }

    input = 0;
    if (g_equip_menu_substate < 2) {
        input = g_menu_input_primary_repeat;
        g_equip_input_page_scroll_disabled = 0;
    } else {
        g_equip_input_page_scroll_disabled = 1;
    }
    override = 0;
    if (g_menu_input_lock_mode != 0 || thread_is_running(2) != 0) {
        override = 1;
    }
    menu_run_script_with_palette_mode(&g_equip_equipment_render_commands, input, override);
    equip_menu_draw_equipment_slot_marker(0, g_equip_selected_slot, g_menu_input_lock_mode);
    if (g_equip_menu_list_entry_count != 0) {
        item_id = g_equip_item_list_entries[g_equip_menu_selected_list_index];
        if (item_id < ITEM_ID_THROWABLE_FIRST) {
            menu_draw_weapon_hand_icons(item_id);
        }
        if (g_equip_item_list_entries[g_equip_menu_selected_list_index] >= ITEM_ID_HEADGEAR_FIRST
            && g_equip_item_list_entries[g_equip_menu_selected_list_index] <= ITEM_ID_ACCESSORY_FIRST - 1) {
            menu_run_script_with_palette_mode(&g_equip_armor_bonus_render_commands, 0, 0);
        }
        equip_menu_draw_item_status_list(g_equip_item_list_entries[g_equip_menu_selected_list_index]);
    }
    return 1;
}
