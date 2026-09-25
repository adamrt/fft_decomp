#include "fft/world.h"
#include "psx/types.h"

void world_menu_start_system_function_thread(s32 parameter) {
    world_menu_clear_entry_flags();
    world_thread_start(4, world_menu_run_system_function_thread);
    world_thread_set_parameters(4, parameter, 0, 0);
}
