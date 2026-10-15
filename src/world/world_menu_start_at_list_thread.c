#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Runs the g_menu_at_list_state menu on the thread two below the current one and
 * waits until that thread and the one below it have finished. */
void world_menu_start_at_list_thread(void) {
    menu_entry_t* list = &g_menu_at_list_state;
    menu_entry_t* saved = g_menu_thread_menu_data;

    g_menu_at_list_state.select_text_table = 0x13;
    g_menu_thread_menu_data = list;
    g_menu_at_list_state.context = &g_world_menu_at_list_entry_table;
    menu_init_at_list(list);
    thread_start(g_thread_current_id - 2, world_build_at_list);
    thread_set_parameters(g_thread_current_id - 2, (s32)list, 0, 0);
    do {
        thread_yield();
    } while (thread_is_running_asm_variant(g_thread_current_id - 2) != 0
        || thread_is_running_asm_variant(g_thread_current_id - 3) != 0);
    g_menu_thread_menu_data = saved;
    thread_exit_current();
}
