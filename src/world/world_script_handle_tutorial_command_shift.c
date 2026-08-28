#include "fft/world.h"
#include "psx/types.h"

void world_script_handle_tutorial_command_shift(void) {
    if (g_world_script_tutorial_command_active != 0) {
        g_world_script_tutorial_command_active -= world_gfx_get_vsync_mode_or_one();
        g_world_script_tutorial_shift_command_bit = 0;
        if (g_world_script_tutorial_command_active <= 0) {
            g_world_script_tutorial_command_active = 0;
            ++g_world_script_tutorial_command_ptr;
        }
    } else {
        g_world_script_tutorial_command_active = 0x1E;
        g_world_script_tutorial_shift_command_bit = 1 << *g_world_script_tutorial_command_ptr;
    }
}
