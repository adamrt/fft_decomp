#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Fitting-room menu step: on entry clear the trial equipment and its cost;
 * then either browse the formation (confirm opens the fitting menu, cancel
 * returns to shop step 0xC) or run the fitting menu thread and dispatch its
 * selection to the next shop step. */
void world_shop_run_fitting_room_step(void) {
    s32 busy;

    if (g_world_shop_fitting_room_initialized == 0) {
        if (g_menu_input_newly_pressed == 0) {
            world_shop_run_obtain_gil_menu();
            return;
        }
        world_formation_init_cursor_and_scroll_state(0);
        memset(g_world_shop_fitting_room_items, 0, sizeof(g_world_shop_fitting_room_items));
        world_shop_add_fitting_room_cost(-world_shop_add_fitting_room_cost(0));
        g_world_shop_fitting_room_unit_chosen = 0;
        g_world_shop_cost_window_visible = 0;
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, -1, 0);
        g_world_shop_fitting_room_initialized = 1;
        return;
    }
    if (g_world_shop_fitting_room_unit_chosen == 0) {
        g_world_shop_background_visible = 0;
        if (g_world_formation_unit_banner_enabled != 0) {
            g_menu_help_message_text_id = (g_world_formation_unit_banner_at_bottom != 0) ? MENU_GRID_HELP_BANNER_BOTTOM
                                                                                         : MENU_GRID_HELP_BANNER_TOP;
        } else {
            g_menu_help_message_text_id = 0;
        }
        busy = world_input_get_lockout_timer();
        busy += world_formation_is_unit_list_scroll_needed();
        if ((g_menu_input_primary_repeat & PSX_PAD_CROSS) && busy == 0) {
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_formation_unit_banner_enabled = 0;
            g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_CHECKOUT;
        } else if ((g_menu_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE)) && busy == 0) {
            g_world_formation_unit_cycle_mode = 2;
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_world_shop_fitting_room_unit_chosen = 1;
            g_world_formation_unit_browse_enabled = 1;
            g_world_formation_unit_banner_at_bottom = 0;
            g_world_shop_cost_window_visible = 1;
            g_world_shop_fitting_room_menu_thread_data[0].selected_index = 0;
            g_world_shop_fitting_room_menu_thread_data[1].selected_index = 0;
            menu_clear_selection_records();
        } else {
            world_formation_update_and_draw_unit_grid(1, 1, 0, g_menu_input_secondary_repeat, g_menu_input_lock_mode,
                world_shop_fitting_room_get_unit_marker, 0);
        }
    } else {
        g_world_shop_background_visible = 1;
        g_menu_help_message_text_id = 0;
        if (menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_world_shop_fitting_room_menu_thread_data) == 0) {
            if (g_world_menu_selection_results[1] == 0) {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_EQUIP;
            } else if (g_world_menu_selection_results[1] == 1) {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_BEST_FIT;
            } else if (g_world_menu_selection_results[1] == 2) {
                g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_REMOVE;
            } else if (g_world_menu_selection_results[1] == 3) {
                if (g_world_menu_selection_results[2] == 0) {
                    g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_SELL_EQUIPPED;
                } else {
                    g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_SELL;
                }
            } else {
                g_world_shop_fitting_room_unit_chosen = 0;
                world_formation_init_selection_cursor(g_unit_selected_index);
                g_world_formation_unit_cycle_mode = 0;
                g_world_formation_unit_browse_enabled = 0;
                g_world_shop_cost_window_visible = 0;
                if (g_world_menu_selection_results[1] == 4) {
                    g_world_formation_unit_banner_enabled = 0;
                    g_world_shop_menu_step = WORLD_SHOP_STEP_FITTING_ROOM_CHECKOUT;
                }
            }
            menu_init_selection_value(WORLD_MENU_CURSOR_REMOVE_SLOT, 0);
            menu_init_selection_value(WORLD_MENU_CURSOR_EQUIP_SLOT, 0);
        }
    }
    if (g_world_shop_cost_window_visible != 0) {
        world_shop_install_callbacks_and_run();
    }
}
