#include "fft/world.h"
#include "psx/types.h"

void world_menu_stop_thread_and_wait(s32 thread_id) {
    u32 unused[2];

    while (world_thread_is_running(thread_id) != 0) {
        world_menu_stop_unit_status_banner_thread(thread_id);
        world_script_update_event_frame_input(unused, 0, 0);
    }
    g_world_menu_thread_running_status = 0;
}
