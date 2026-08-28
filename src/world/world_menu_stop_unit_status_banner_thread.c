#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

void world_menu_stop_unit_status_banner_thread(s32 thread_id) {
    world_thread_set_parameters(thread_id, 0, 0, 1);
}
