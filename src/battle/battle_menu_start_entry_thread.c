#include "fft/battle.h"
#include "fft/script_variables.h"
#include "psx/types.h"

/* Menu thread: waits for thread 8, runs the tutorial event if pending, then
 * starts the menu-entry thread for the requested menu (0x10/0x12 redirect to
 * 0x36 for monster units) and optionally the two follow-up text threads.
 *
 * BATTLE twin of world_menu_start_entry_thread. */
void battle_menu_start_entry_thread(void) {
    s32 menu_id = battle_thread_get_current_parameter_1();
    s32 count;

    battle_thread_wait_until_inactive(8);
    if (g_option_menu_submenu_state != 0) {
        battle_thread_exit_current();
    }
    battle_camera_wait_until_idle();
    while (battle_thread_find_running_by_task(0x12) != 0) {
        battle_thread_yield();
    }
    if (g_battle_menu_input_disabled == 0) {
        if (battle_script_is_tutorial_event_slot() != 0) {
            g_battle_menu_input_disabled = 2;
            battle_script_set_variable(
                EVENT_SCRIPT_VAR_CURRENT_EVENT, battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) + 1);
            battle_script_start_current_event_thread();
            battle_thread_wait_frames(0x1E);
        }
        if (g_battle_menu_input_disabled != 0) {
            battle_thread_wait_frames(2);
        }
    } else {
        battle_thread_wait_frames(2);
    }
    battle_script_pulse_tutorial_wait_value(0xFF);
    if (menu_id == 0x10 || menu_id == 0x12) {
        if (battle_unit_get_attacker_data_pointer()->unit_flags & UNIT_FLAG_EGG) {
            menu_id = 0x36;
        }
    }
    battle_thread_start(8, g_battle_menu_thread_menu_data[menu_id].thread_entry);
    battle_thread_set_parameters(8, (s32)&g_battle_menu_thread_menu_data[menu_id], 0, 0);
    count = g_battle_menu_restore_depth;
    g_battle_script_event_input = 0;
    if (g_battle_menu_restore_pending != 0 && g_battle_menu_restore_unit_id == g_battle_active_turn_unit.battle_id) {
        g_battle_menu_help_open = 1;
        battle_thread_wait_frames(2);
        if (count > 0) {
            battle_thread_start(7, battle_menu_run_skillset_thread);
            battle_thread_set_parameters(7, (s32)&g_battle_menu_thread_menu_data[4], 0, 0);
        }
        battle_thread_wait_frames(2);
        if (count >= 2) {
            battle_thread_start(6, battle_menu_select_unit_action_slots_thread);
            battle_thread_set_parameters(6, (s32)&g_battle_menu_thread_menu_data[3], 0, 0);
        }
        battle_thread_wait_frames(2);
        g_battle_menu_restore_pending = 0;
        g_battle_menu_help_open = 0;
    } else {
        g_battle_menu_restore_depth = 0;
        g_battle_menu_restore_unit_id = g_battle_active_turn_unit.battle_id;
    }
    battle_thread_exit_current();
}
