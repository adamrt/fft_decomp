#include "fft/thread.h"
#include "fft/world.h"

/*
 * Dispatch formation item-menu transitions and its submenus (6:
 * world_formation_run_equip_item_menu, 7: best-fit equipment, 8: remove item, 9:
 * world_formation_run_item_submenu_step).
 *
 * States 0x10/0x17 slide the grid in, 0x11 slides it back out; state 1 runs
 * the icon list menu (g_world_item_icon_menu) and enters submenu 6..9 from the selected
 * row, refreshing the ordered unit list by roster slot when the list closes.
 * Units world_formation_get_unit_lock reports as locked get text 0xD802.
 */
void world_formation_item_menu(void) {
    s32 state;
    s8 running;
    u8 saved;
    s32 done;
    s32 roster_slot;
    s16* selection;

    state = g_world_formation_current_menu;
    if (state == WORLD_FORMATION_MENU_ITEM_ENTER || state == WORLD_FORMATION_MENU_ITEM_ENTER_NO_SLIDE) {
        if (g_world_item_menu_initialized == 0) {
            saved = g_world_formation_unit_browse_enabled;
            g_world_formation_unit_banner_at_bottom = 0;
            g_world_formation_unit_browse_enabled = 0;
            g_world_item_menu_saved_browse_enabled = saved;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
            g_world_item_menu_initialized = 1;
        }
        if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ITEM_ENTER) {
            g_world_item_menu_initialized = world_formation_step_grid_transition(0);
        } else {
            g_world_item_menu_initialized = 0;
        }
        if (g_world_item_menu_initialized == 0) {
            g_world_formation_current_menu = WORLD_FORMATION_MENU_ITEM;
        }
        return;
    }
    if (state == WORLD_FORMATION_MENU_ITEM_LEAVE) {
        running = world_formation_step_grid_return_transition(1);
        g_world_item_menu_initialized = running;
        if (running == 0) {
            g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
            g_world_formation_triangle_menu.selected_index = 0;
        }
        return;
    }
    if (g_world_item_menu_initialized == 0 && thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU) == 0) {
        g_world_item_icon_menu.selected_index = 0;
        g_menu_thread_menu_data = &g_world_item_icon_menu;
        thread_start(WORLD_THREAD_SLOT_UPPER_MENU, menu_icon_list_thread);
        thread_set_parameters(WORLD_THREAD_SLOT_UPPER_MENU, (s32)g_menu_thread_menu_data, 0, 0);
        g_world_item_menu_unit_index = 0xFF;
        menu_clear_selection_records();
        g_world_formation_unit_banner_enabled = 1;
        g_world_item_submenu_exit_pending = 0;
        g_world_item_menu_initialized = 1;
    }
    if (g_world_formation_current_menu < 9) {
        world_formation_place_unit_portrait_at_rest(g_unit_selected_index);
    }
    if (g_world_item_menu_unit_index != g_unit_selected_index) {
        g_world_item_menu_unit_index = (u8)g_unit_selected_index;
        if (world_formation_get_unit_lock(g_unit_selected_index) < 0) {
            g_world_item_icon_menu.text_id = 0xD802;
        } else {
            g_world_item_icon_menu.text_id = 0xD801;
        }
        g_world_menu_text_redraw_request = 1;
    }
    if (g_world_item_submenu_exit_pending != 0) {
        thread_suspend(WORLD_THREAD_SLOT_UPPER_MENU_CHILD);
        g_world_item_submenu_exit_pending = 0;
    }
    if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ITEM) {
        g_world_formation_unit_cycle_mode = 2;
        if (g_world_menu_selection_results[1] >= 0 && g_world_menu_selection_results[1] < 3
            && world_formation_get_unit_lock(g_unit_selected_index) < 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
            world_menu_reset_selection_results();
            return;
        }
        selection = &g_world_menu_selection_results[1];
        if (*selection != -1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            thread_start(WORLD_THREAD_SLOT_UPPER_MENU_CHILD, world_formation_item_menu_idle_thread);
            g_world_formation_current_menu = *selection + WORLD_FORMATION_MENU_ITEM_EQUIP;
            menu_init_selection_value(WORLD_MENU_CURSOR_EQUIP_SLOT, 0);
            menu_init_selection_value(WORLD_MENU_CURSOR_REMOVE_SLOT, 0);
            menu_init_selection_value(WORLD_MENU_CURSOR_ITEM_TAB, 0);
        }
        running = thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU);
        g_world_item_menu_initialized = running;
        if (running == 0) {
            world_menu_toggle_change_banner_panel_thread(0);
            roster_slot = g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot;
            world_formation_build_record_list(0, g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
            g_unit_selected_index = world_formation_find_index_by_roster_slot(roster_slot);
            g_world_formation_current_menu = WORLD_FORMATION_MENU_ITEM_LEAVE;
            g_world_formation_unit_browse_enabled = g_world_item_menu_saved_browse_enabled;
            g_world_formation_unit_banner_enabled = g_world_formation_saved_banner_enabled;
        }
        return;
    }
    switch (g_world_formation_current_menu) {
    case WORLD_FORMATION_MENU_ITEM_EQUIP:
        done = world_formation_run_equip_item_menu();
        break;
    case WORLD_FORMATION_MENU_ITEM_BEST_FIT:
        done = world_menu_run_best_fit_equipment();
        break;
    case WORLD_FORMATION_MENU_ITEM_REMOVE:
        done = world_menu_run_remove_item();
        break;
    case WORLD_FORMATION_MENU_ITEM_LIST:
        done = world_formation_run_item_submenu_step();
        break;
    }
    if (done == 0) {
        g_world_item_menu_unit_index = 0xFF;
        world_formation_stage_selected_unit();
        g_world_formation_current_menu = WORLD_FORMATION_MENU_ITEM;
        g_world_item_submenu_exit_pending = 1;
    }
}
