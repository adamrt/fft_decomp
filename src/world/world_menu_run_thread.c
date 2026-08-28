#include "fft/world.h"

s32 world_menu_run_thread(s32 thread_id, void* arg) {
    s32 status;
    if (g_world_menu_thread_running_status == 0) {
        if (world_thread_is_running(thread_id) != 0) {
            return 1;
        }
        {
            void (*fn)(void) = ((world_menu_entry_t*)arg)->thread_entry;
            g_world_menu_thread_menu_data = (world_menu_entry_t*)arg;
            world_thread_start(thread_id, fn);
        }
        world_thread_set_parameters(thread_id, (s32)g_world_menu_thread_menu_data, 0, 0);
        g_world_menu_thread_running_status = 1;
        return 1;
    }
    status = world_thread_is_running(thread_id);
    g_world_menu_thread_running_status = status;
    return status;
}
