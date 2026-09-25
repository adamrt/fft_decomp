#include "fft/world.h"

void world_thread_exit_current(void) {
    g_world_threads[g_world_thread_current_id].is_running = 0;
    g_world_threads[g_world_thread_current_id].task_id = 0;
    world_thread_yield();
}
