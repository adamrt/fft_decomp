#include "fft/event_equip.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Update the secondary item-action menu and return its next screen state. */
s32 equip_menu_update_item_action_state(void) {
    world_menu_entry_t* menu;

    if (g_equip_item_action_menu_active == 0) {
        if (g_equip_menu_list_entry_count == 0) {
            return 0;
        }
        g_equip_item_action_menu.selected_index = 0;
        g_equip_item_action_menu_single_item.selected_index = 0;
        g_equip_text_help_message_id = 0;
        equip_menu_reset_selection_indices();
        g_equip_item_numeric_panel_style = 0;
        if (g_equip_item_list_entries[1] != -1) {
            g_equip_item_list_has_multiple_entries = 1;
            menu = &g_equip_item_action_menu;
        } else {
            menu = &g_equip_item_action_menu_single_item;
            g_equip_item_list_has_multiple_entries = 0;
        }
        g_equip_item_action_active_menu = menu;
        g_equip_item_action_menu_active = 1;
    }

    g_equip_text_help_message_id = g_equip_item_action_active_menu->selected_index + 0x102F;
    g_equip_item_action_menu_active = equip_thread_start_managed(3, g_equip_item_action_active_menu);
    if (g_equip_item_action_menu_active == 0) {
        return 0;
    }
    if (g_equip_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_equip_item_action_menu.selected_index == 2) {
            g_equip_item_action_menu_active = 0;
            equip_thread_stop_and_clear_state(3);
            return 5;
        }
        g_equip_sound_queued_effect_id = MAIN_SFX_INVALID;
    }
    return 2;
}
