/* One frame of the equipment menu's state machine. */

#include "fft/equipment.h"
#include "fft/gfx_image.h"
#include "fft/input.h"
#include "fft/item.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit_panel.h"
#include "psx/types.h"

/* Runs one frame of the equipment menu state machine.
 *
 * The first call suspends the command menu thread, stops the status panels and
 * sets up the list menu. Each frame runs the current sub-state handler, which
 * returns the next state; MENU_ITEM_LIST_STATE_CLOSE tears the menu down, restarts
 * the panels, resumes the command menu and returns 0. Otherwise the menu, slot
 * marker and selected-item panels are drawn and 1 is returned.
 */
s32 equipment_update_menu(void) {
    s32 input;
    s32 override;
    s16 item_id;
    u8 unused[0x18]; /* never accessed; reserves the target's 0x18-byte frame area */

    if (g_equipment_ready == 0) {
        g_equipment_ready = 1;
        g_menu_selected_slot = menu_init_selection_value(2, 0);
        thread_suspend(EQUIPMENT_THREAD_SLOT_COMMAND_MENU);
        menu_request_thread_stop(EQUIPMENT_THREAD_SLOT_UNIT_STATUS);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
        menu_request_thread_stop(EQUIPMENT_THREAD_SLOT_EQUIPMENT_LIST);
        menu_init_scrollable_list_core(0, 0, 0);
        g_equipment_substate = MENU_ITEM_LIST_STATE_BROWSE;
        g_equipment_unit_banner_enabled = 0;
        g_unit_panel_preview_stats_thread_params.x = 0;
        g_unit_panel_preview_stats_thread_params.y = -0x49;
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
        g_unit_panel_preview_stats_thread_params.x = 2;
        g_unit_panel_preview_stats_thread_params.y = 2;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        return 0;
    }
    if (g_equipment_substate == MENU_ITEM_LIST_STATE_BROWSE) {
        g_equipment_substate = equipment_run_item_list();
    } else if (g_equipment_substate == MENU_ITEM_LIST_STATE_REARRANGE) {
        g_equipment_substate = item_run_rearrange_mode();
    } else if (g_equipment_substate == MENU_ITEM_LIST_STATE_ACTION) {
        g_equipment_substate = equipment_update_item_actions();
    } else if (g_equipment_substate == MENU_ITEM_LIST_STATE_SORT_ORDER) {
        g_equipment_substate = equipment_run_type_order_mode();
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
    menu_run_script_with_palette_mode(&g_equipment_equipment_render_commands, input, override);
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
            menu_run_script_with_palette_mode(&g_equipment_armor_bonus_render_commands, 0, 0);
        }
        item_draw_status_list(g_item_list_entries[g_menu_list_selected_index]);
    }
    return 1;
}
