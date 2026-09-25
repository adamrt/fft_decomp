#include "fft/battle.h"
#include "psx/types.h"

/* BATTLE twin of world_menu_run_skillset_thread: builds the skillset menu on the main stack,
 * then either waits on the child menu thread or runs the icon selection loop
 * for the selected unit and stores its result byte.
 *
 * As in the WORLD twin, the completion value is materialised before the
 * branch (`li v1,1` in the beqz delay slot) and stored through a pointer to
 * the thread context, which keeps the record loads after that store.
 */
void battle_menu_run_skillset_thread(void) {
    s32 param_value;
    battle_menu_record_t* record;
    s32* param;

    battle_menu_check_action_menu_restrictions();
    g_battle_thread_call_target = battle_menu_build_skillset_entries;
    battle_thread_call_on_main_stack();
    param_value = 1;
    if (g_battle_menu_action_menu_build_result != 0) {
        g_battle_menu_thread_menu_data[4].selected_index = 0;
        if (g_battle_menu_restore_pending == 0) {
            battle_thread_start(g_battle_current_thread_id - 1, battle_menu_select_unit_action_slots_thread);
            battle_thread_set_parameters(g_battle_current_thread_id - 1, (s32)&g_battle_menu_thread_menu_data[3], 0, 0);
        } else {
            battle_thread_wait_frames(2);
        }
        for (;;) {
            battle_thread_yield();
            if (battle_thread_is_running_8014cc94(g_battle_current_thread_id - 1) == 0) {
                battle_thread_exit_current();
            }
        }
    }
    param = &g_battle_thread_contexts[g_battle_current_thread_id].function_parameter_2;
    *param = param_value;
    record = &g_battle_menu_unit_selection_records[g_battle_active_turn_unit.battle_id];
    g_battle_menu_thread_menu_data[4].selected_index = record->bytes[2];
    g_battle_menu_restore_depth = 1;
    battle_script_pulse_tutorial_wait_value(0xFC);
    battle_menu_run_icon_selection_loop();
    record->bytes[2] = g_battle_menu_thread_menu_data[4].selected_index;
    battle_thread_exit_current();
}
