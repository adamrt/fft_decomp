#include "fft/world.h"
#include "psx/types.h"

void world_script_handle_tutorial_command_wait_time(void) {
    if (g_world_script_tutorial_command_active != 0) {
        g_world_script_tutorial_wait_time_remaining -= world_gfx_get_vsync_mode_or_one();
        if (g_world_script_tutorial_wait_time_remaining < 0) {
            g_world_script_tutorial_command_active = 0;
            g_world_script_tutorial_command_ptr += 2;
        }
    } else {
        g_world_script_tutorial_command_active = 1;
        g_world_script_tutorial_wait_time_remaining = *(g_world_script_tutorial_command_ptr + 1);
    }
}
