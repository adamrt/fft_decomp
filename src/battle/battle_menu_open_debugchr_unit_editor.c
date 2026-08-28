#include "fft/battle.h"
#include "fft/debugchr.h"
#include "fft/option.h"
#include "psx/types.h"

void battle_menu_open_debugchr_unit_editor(void) {
    s16* selected = &g_battle_active_turn_unit.battle_id;

    if (battle_unit_get_stats_from_battle_id(*selected)->mount_info == 0) {
        return;
    }
    battle_menu_store_unit_names_and_event_block_data(0, 0xFF, 0xFF);
    battle_menu_set_option_menu_open();
    g_option_menu_submenu_state = 1;
    battle_menu_request_open_companion_executable(0xB);
    battle_thread_set_parameters(8, 0, 0, 1);
    battle_thread_wait_until_inactive(8);
    battle_thread_wait_for_10_to_13();
    battle_menu_record_selection_for_selected_unit();
    battle_menu_enter_status_screen_selection();
    debugchr_editor_run_unit_thread(*selected);
    g_option_menu_submenu_state = 0;
    g_battle_sound_suppressed = 0;
    g_battle_script_event_input = 0;
    battle_menu_build_idle_action_menu(g_battle_menu_current_id);
    battle_menu_store_unit_names_and_event_block_data(3, *selected, *selected);
    battle_menu_clear_option_menu_open();
    battle_menu_leave_status_screen_selection();
    battle_thread_exit_current();
}
