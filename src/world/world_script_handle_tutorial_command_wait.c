#include "fft/world.h"
#include "psx/types.h"

void world_script_handle_tutorial_command_wait(void) {
    g_world_script_tutorial_wait_timer -= world_gfx_get_vsync_mode_or_one();
    if (g_world_script_tutorial_wait_timer <= 0) {
        g_world_script_tutorial_wait_timer = 0;
        g_world_script_tutorial_command_ptr++;
    }
}
