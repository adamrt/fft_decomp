#include "fft/event_etc.h"
#include "fft/world.h"
#include "psx/types.h"

void world_script_show_graphic(void) {
    world_thread_set_current_task_id(NATIVE_THREAD_TASK_SHOW_GRAPHIC);
    world_bin_load_file(0xC);
    etc_graphic_show_async();
}
