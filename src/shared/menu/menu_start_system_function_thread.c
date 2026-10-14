#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/thread.h"

/* Clear menu selections and start the system-function worker. */
void menu_start_system_function_thread(s32 system_function) {
    menu_clear_entry_selections();
    thread_start(THREAD_SLOT_SYSTEM_FUNCTION, battle_menu_run_system_function_thread);
    thread_set_parameters(THREAD_SLOT_SYSTEM_FUNCTION, system_function, 0, 0);
}
