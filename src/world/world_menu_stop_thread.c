#include "fft/world.h"
#include "psx/types.h"

void world_menu_stop_thread(s32 thread_id) {
    world_menu_stop_unit_status_banner_thread(thread_id);
    g_world_menu_thread_running_status = 0;
}
