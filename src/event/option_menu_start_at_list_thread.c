#include "fft/option.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Install the AT-list menu context until both builder threads finish. */
void option_menu_start_at_list_thread(void) {
    world_menu_entry_t* previous_menu_descriptor = g_battle_menu_thread_menu_data;

    g_option_menu_at_list_menu_type = 0x13;
    g_battle_menu_thread_menu_data = (world_menu_entry_t*)g_option_menu_at_list_state;
    g_option_menu_at_list_entry_table_pointer = g_option_menu_at_list_entry_table;
    option_menu_init_at_list(g_option_menu_at_list_state);

    battle_thread_start(g_battle_current_thread_id - 2, option_build_at_list);
    battle_thread_set_parameters(g_battle_current_thread_id - 2, g_option_menu_at_list_state, 0, 0);

    do {
        battle_thread_yield();
    } while (battle_thread_is_running_8014cc94(g_battle_current_thread_id - 2) != 0
        || battle_thread_is_running_8014cc94(g_battle_current_thread_id - 3) != 0);

    g_battle_menu_thread_menu_data = previous_menu_descriptor;
    battle_thread_exit_current();
}
