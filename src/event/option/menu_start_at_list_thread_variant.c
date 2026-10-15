#include "fft/event_option.h"
#include "fft/thread.h"

/* Install the AT-list menu context until both builder threads finish. */
void menu_start_at_list_thread_variant(void) {
    menu_entry_t* previous_menu_descriptor = g_menu_thread_menu_data;

    g_option_menu_at_list_menu_type = 0x13;
    g_menu_thread_menu_data = &g_menu_at_list_state;
    g_option_menu_at_list_entry_table_pointer = g_option_menu_at_list_entry_table;
    menu_init_at_list(&g_menu_at_list_state);

    thread_start(g_thread_current_id - 2, menu_run_at_list_variant);
    thread_set_parameters(g_thread_current_id - 2, (s32)&g_menu_at_list_state, 0, 0);

    do {
        thread_yield();
    } while (thread_is_running_asm_variant(g_thread_current_id - 2) != 0
        || thread_is_running_asm_variant(g_thread_current_id - 3) != 0);

    g_menu_thread_menu_data = previous_menu_descriptor;
    thread_exit_current();
}
