#include "fft/world.h"
#include "psx/types.h"

void world_menu_stop_thread(s32 thread_id) {
    menu_request_thread_stop(thread_id);
    g_menu_managed_thread_state = 0;
}
