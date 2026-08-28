#include "fft/data.h"
#include "fft/thread.h"
#include "fft/world.h"

/* Runs the main-menu mode-0 thread in the slot below the current thread,
 * handing it this thread's first parameter, then waits for it and stops. */
/* Target 0x80106f64. */
void world_menu_run_main_mode_0_in_previous_thread(void) {
    s32 thread_id;

    world_thread_start(g_world_thread_current_id - 1, world_menu_run_main_mode_0_thread);
    thread_id = g_world_thread_current_id;
    world_thread_set_parameters(thread_id - 1, g_world_threads[thread_id].function_parameter_1, 0, 0);
    world_thread_wait_until_inactive(g_world_thread_current_id - 1);
    world_thread_exit_current();
}
