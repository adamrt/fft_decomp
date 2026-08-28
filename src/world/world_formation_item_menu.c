#include "fft/data.h"
#include "fft/main_sound.h"
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
 * Units locked by world_formation_lock_equipment_and_abilities get text 0xD802.
 */
void world_formation_item_menu(void) {
    s32 state;
    s8 running;
    u8 saved;
    s32 done;
    s32 roster_slot;
    s16* selection;

    state = g_world_formation_current_menu;
    if (state == 0x10 || state == 0x17) {
        if (g_world_item_menu_initialized == 0) {
            saved = g_world_formation_unit_browse_enabled;
            g_world_formation_scroll_enabled = 0;
            g_world_formation_unit_browse_enabled = 0;
            g_world_item_menu_saved_browse_enabled = saved;
            world_menu_stop_unit_status_banner_thread(9);
            g_world_item_menu_initialized = 1;
        }
        if (g_world_formation_current_menu == 0x10) {
            g_world_item_menu_initialized = world_formation_step_grid_transition(0);
        } else {
            g_world_item_menu_initialized = 0;
        }
        if (g_world_item_menu_initialized == 0) {
            g_world_formation_current_menu = 1;
        }
        return;
    }
    if (state == 0x11) {
        running = world_formation_step_grid_return_transition(1);
        g_world_item_menu_initialized = running;
        if (running == 0) {
            g_world_formation_current_menu = 0;
            g_world_formation_triangle_menu.cursor = 0;
        }
        return;
    }
    if (g_world_item_menu_initialized == 0 && world_thread_is_running(15) == 0) {
        g_world_item_icon_menu.selected_index = 0;
        g_world_menu_thread_menu_data = &g_world_item_icon_menu;
        world_thread_start(15, world_menu_icon_list_thread);
        world_thread_set_parameters(15, (s32)g_world_menu_thread_menu_data, 0, 0);
        g_world_item_menu_unit_index = 0xFF;
        world_gfx_clear_sprite_slots();
        g_world_formation_unit_banner_enabled = 1;
        g_world_item_submenu_exit_pending = 0;
        g_world_item_menu_initialized = 1;
    }
    if (g_world_formation_current_menu < 9) {
        world_formation_place_unit_portrait_at_rest(g_world_formation_selected_unit_index);
    }
    if (g_world_item_menu_unit_index != g_world_formation_selected_unit_index) {
        g_world_item_menu_unit_index = ((world_menu_halfword_view_t*)&g_world_formation_selected_unit_index)->bytes[0];
        if (world_formation_lock_equipment_and_abilities(g_world_formation_selected_unit_index) < 0) {
            g_world_item_icon_menu.text_id = 0xD802;
        } else {
            g_world_item_icon_menu.text_id = 0xD801;
        }
        g_world_menu_text_redraw_request = 1;
    }
    if (g_world_item_submenu_exit_pending != 0) {
        world_thread_suspend(14);
        g_world_item_submenu_exit_pending = 0;
    }
    if (g_world_formation_current_menu == 1) {
        g_world_formation_unit_cycle_mode = 2;
        if ((u16)g_world_menu_selection_results[1] < 3
            && world_formation_lock_equipment_and_abilities(g_world_formation_selected_unit_index) < 0) {
            g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
            world_menu_reset_selection_results();
            return;
        }
        selection = &g_world_menu_selection_results[1];
        if (*selection != -1) {
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            world_thread_start(14, world_thread_idle_wait_forever);
            g_world_formation_current_menu = *selection + 6;
            world_menu_set_cursor_position(2, 0);
            world_menu_set_cursor_position(3, 0);
            world_menu_set_cursor_position(7, 0);
        }
        running = world_thread_is_running(15);
        g_world_item_menu_initialized = running;
        if (running == 0) {
            world_menu_toggle_change_banner_panel_thread(0);
            roster_slot = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->roster_slot;
            world_formation_build_record_list(0, g_world_formation_unit_pointers, 0);
            g_world_formation_selected_unit_index = world_formation_find_index_by_roster_slot(roster_slot);
            g_world_formation_current_menu = 0x11;
            g_world_formation_unit_browse_enabled = g_world_item_menu_saved_browse_enabled;
            g_world_formation_unit_banner_enabled = g_world_formation_saved_banner_enabled;
        }
        return;
    }
    switch (g_world_formation_current_menu) {
    case 6:
        done = world_formation_run_equip_item_menu();
        break;
    case 7:
        done = world_menu_run_best_fit_equipment();
        break;
    case 8:
        done = world_menu_run_remove_item();
        break;
    case 9:
        done = world_formation_run_item_submenu_step();
        break;
    }
    if (done == 0) {
        g_world_item_menu_unit_index = 0xFF;
        world_formation_stage_selected_unit();
        g_world_formation_current_menu = 1;
        g_world_item_submenu_exit_pending = 1;
    }
}
