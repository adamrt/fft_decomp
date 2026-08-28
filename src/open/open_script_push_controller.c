#include "fft/open.h"
#include "psx/types.h"

void open_script_push_controller(void) {
    s32 controller;

    g_open_system_runtime_flags &= ~(OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN | OPEN_SYSTEM_RUNTIME_FLAG_RGB24);
    open_gfx_init_screen_environments(1);
    g_open_system_runtime_flags |= 0x40000;
    open_script_init_state();

    controller = g_open_current_controller_index;
    g_open_system_vsync_mode = 2;
    g_open_controller_handler_indices[controller] = 10;
    g_open_current_controller_index = controller + 1;
}
