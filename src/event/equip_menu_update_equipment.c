#include "fft/event_equip.h"

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
        g_equip_selected_slot = equip_menu_set_selection_value_2(2, 0);
        battle_thread_suspend(5);
        equip_thread_request_stop(0xD);
        equip_thread_request_stop(0xC);
        equip_thread_request_stop(0xA);
        equip_menu_init_scrollable_list_core(0, 0, 0);
        g_equip_menu_substate = 0;
        g_equip_unit_banner_enabled = 0;
        g_equip_item_numeric_panel_params = 0;
        g_equip_item_numeric_panel_y = -0x49;
        equip_gfx_load_image_and_wait(&g_equip_status_label_image_rect, (u32*)g_equip_status_label_image);
    }
    if (g_equip_menu_substate == -1) {
        equip_panel_toggle_item_numeric_thread(0);
        equip_panel_toggle_equipment_threads(1);
        equip_panel_toggle_numeric_thread(1);
        equip_panel_toggle_unit_status_thread(1);
        g_equip_menu_ready = 0;
        g_equip_unit_banner_enabled = 1;
        battle_thread_resume(5);
        g_equip_item_numeric_panel_params = 2;
        g_equip_item_numeric_panel_y = 2;
        g_equip_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    if (g_equip_menu_substate == 0) {
        g_equip_menu_substate = equip_menu_run_slot_item_browser();
    } else if (g_equip_menu_substate == 1) {
        g_equip_menu_substate = equip_menu_run_item_rearrange_mode();
    } else if (g_equip_menu_substate == 2) {
        g_equip_menu_substate = equip_menu_update_item_action_state();
    } else if (g_equip_menu_substate == 5) {
        g_equip_menu_substate = equip_run_item_type_order_mode();
    }

    input = 0;
    if (g_equip_menu_substate < 2) {
        input = g_equip_input_primary_repeat;
        g_equip_input_page_scroll_disabled = 0;
    } else {
        g_equip_input_page_scroll_disabled = 1;
    }
    override = 0;
    if (g_event_mode != 0 || battle_thread_is_running(2) != 0) {
        override = 1;
    }
    equip_menu_dispatch_with_override((s32)g_equip_equipment_render_commands, input, override);
    equip_menu_draw_equipment_slot_marker(0, g_equip_selected_slot, g_event_mode);
    if (g_equip_menu_list_entry_count != 0) {
        item_id = g_equip_item_list_entries[(s16)g_equip_menu_selected_list_index];
        if (item_id < ITEM_ID_THROWABLE_FIRST) {
            equip_menu_draw_weapon_hand_icons(item_id);
        }
        if ((u16)(g_equip_item_list_entries[(s16)g_equip_menu_selected_list_index] - 0x90) < 0x40) {
            equip_menu_dispatch_with_override((s32)g_equip_armor_bonus_render_commands, 0, 0);
        }
        equip_menu_draw_item_status_list(g_equip_item_list_entries[(s16)g_equip_menu_selected_list_index]);
    }
    return 1;
}
