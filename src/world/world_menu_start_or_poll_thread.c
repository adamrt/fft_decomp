#include "fft/world.h"
#include "psx/types.h"

/* Restart the menu thread with the given data, waiting 20 frames, or
 * poll the running thread's status. */
void world_menu_start_or_poll_thread(s32 thread_id, world_menu_thread_data_t* data) {
    s32 i;
    u32 unused[2];
    s32 status;

    if (g_world_menu_thread_running_status == 0) {
        world_menu_stop_thread_and_wait(thread_id);
        g_world_menu_thread_menu_data = (world_menu_entry_t*)data;
        world_thread_start(thread_id, data->thread_entry);
        world_thread_set_parameters(thread_id, (s32)g_world_menu_thread_menu_data, 0, 0);
        for (i = 0; i < 20; i++) {
            world_script_update_event_frame_input(unused, 0, 0);
        }
        status = 1;
    } else {
        status = world_thread_is_running(thread_id);
    }
    g_world_menu_thread_running_status = status;
}
