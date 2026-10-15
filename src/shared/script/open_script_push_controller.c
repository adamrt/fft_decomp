#include "fft/open.h"
#include "psx/types.h"

/* Start the ending script: leave the 320-pixel and 24-bit modes, make each
 * frame's background the picture copied from VRAM
 * (OPEN_SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM), reset the script state, which sets
 * up the 512-pixel screen, select VSync mode 2 and push handler 10. */
void open_script_push_controller(void) {
    s32 controller;

    g_open_system_runtime_flags &= ~(OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN | OPEN_SYSTEM_RUNTIME_FLAG_RGB24);
    open_gfx_init_screen_environments(1);
    g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_FRAME_FROM_VRAM;
    open_script_init_state();

    controller = g_open_current_controller_index;
    g_open_system_vsync_mode = 2;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_SCRIPT;
    g_open_current_controller_index = controller + 1;
}
