#include "fft/menu.h"
#include "fft/thread.h"

/* Menu-opening thread 6: the Wait menu. */
void menu_wait_thread(void) {
    menu_entry_t* menu;

    menu = (menu_entry_t*)battle_thread_get_current_parameter_1();
    g_menu_wait_row_actions[0] = -2;
    g_menu_wait_row_actions[1] = -2;
    menu_confirm_action_silently(menu);
    thread_exit_current();
}
