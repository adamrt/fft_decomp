#include "fft/battle.h"
#include "fft/event_debugchr.h"
#include "psx/types.h"

void battle_menu_open_debugchr(void) {
    battle_menu_store_unit_names_and_event_block_data(0, 0xFF, 0xFF);
    battle_menu_set_option_menu_open();
    g_option_menu_submenu_state = 1;
    battle_menu_request_open_companion_executable(0xB);
    battle_thread_set_parameters(8, 0, 0, 1);
    battle_thread_wait_until_inactive(8);
    battle_thread_wait_for_10_to_13();
    battle_thread_yield();
    battle_thread_yield();
    debugchr_run_character_debug_session();
    g_option_menu_submenu_state = 0;
    g_battle_sound_suppressed = 0;
    g_battle_script_event_input = 0;
    battle_menu_clear_option_menu_open();
    battle_thread_exit_current();
}
