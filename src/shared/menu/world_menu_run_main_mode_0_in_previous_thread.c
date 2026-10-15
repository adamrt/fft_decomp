#include "fft/thread.h"
#include "fft/world.h"

/* Runs the main-menu mode-0 thread in the slot below the current thread,
 * handing it this thread's first parameter, then waits for it and stops. */
/* Target 0x80106f64. */
void world_menu_run_main_mode_0_in_previous_thread(void) {
    s32 thread_id;

    thread_start(g_thread_current_id - 1, world_menu_run_main_mode_0_thread);
    thread_id = g_thread_current_id;
    thread_set_parameters(thread_id - 1, g_threads[thread_id].function_parameter_1, 0, 0);
    thread_wait_until_inactive(g_thread_current_id - 1);
    thread_exit_current();
}
