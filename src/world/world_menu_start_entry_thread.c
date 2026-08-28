#include "fft/battle.h"
#include "fft/event.h"
#include "fft/script_variables.h"
#include "fft/world.h"
#include "psx/types.h"

/* Menu thread: waits for thread 8, runs the tutorial event if pending, then
 * starts the menu-entry thread for the requested menu (0x10/0x12 redirect to
 * 0x36 for monster units) and optionally the two follow-up text threads. */
void world_menu_start_entry_thread(void) {
    s32 menu_id = (s32)world_thread_get_current_parameter_1();
    s32 count;

    world_thread_wait_until_inactive(8);
    if (*(u16*)&g_world_menu_overlay_state != 0) {
        world_thread_exit_current();
    }
    world_camera_run_move_thread();
    while (world_thread_find_running_by_task(0x12) != 0) {
        world_thread_yield();
    }
    if (g_world_menu_input_disabled == 0) {
        if (world_script_check_tutorial_event_slot() != 0) {
            g_world_menu_input_disabled = 2;
            world_script_set_variable(
                EVENT_SCRIPT_VAR_CURRENT_EVENT, world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) + 1);
            world_script_start_event_from_variables();
            world_thread_wait_frames(0x1E);
        }
        if (g_world_menu_input_disabled != 0) {
            world_thread_wait_frames(2);
        }
    } else {
        world_thread_wait_frames(2);
    }
    world_script_pulse_tutorial_wait_value(0xFF);
    if (menu_id == 0x10 || menu_id == 0x12) {
        if (world_unit_get_battle_stats_for_stored()->unit_flags & UNIT_FLAG_EGG) {
            menu_id = 0x36;
        }
    }
    world_thread_start(8, g_world_menu_thread_menu_data[menu_id].thread_entry);
    world_thread_set_parameters(8, (s32)&g_world_menu_thread_menu_data[menu_id], 0, 0);
    count = g_world_menu_restore_depth;
    g_world_menu_new_button_input = 0;
    if (g_world_menu_restore_pending != 0 && g_world_menu_restore_unit_id == g_world_unit_view_battle_id) {
        g_world_text_overlay_active = 1;
        world_thread_wait_frames(2);
        if (count > 0) {
            world_thread_start(7, world_menu_run_skillset_thread);
            world_thread_set_parameters(7, (s32)&g_world_menu_thread_menu_data[4], 0, 0);
        }
        world_thread_wait_frames(2);
        if (count >= 2) {
            world_thread_start(6, world_menu_select_unit_action_slots_thread);
            world_thread_set_parameters(6, (s32)&g_world_menu_thread_menu_data[3], 0, 0);
        }
        world_thread_wait_frames(2);
        g_world_menu_restore_pending = 0;
        g_world_text_overlay_active = 0;
    } else {
        g_world_menu_restore_depth = 0;
        g_world_menu_restore_unit_id = g_world_unit_view_battle_id;
    }
    world_thread_exit_current();
}
