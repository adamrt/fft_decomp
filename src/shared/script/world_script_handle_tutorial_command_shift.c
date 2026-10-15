#include "fft/world.h"

/* Tutorial command below WORLD_TUTORIAL_CMD_SHIFT_LIMIT: press pad bit
 * number <command> for one frame, then wait 30 frames (counted by the VSync
 * mode) before moving to the next command. */
void world_script_handle_tutorial_command_shift(void) {
    if (g_world_script_tutorial_command_active != 0) {
        g_world_script_tutorial_command_active -= menu_get_event_speed();
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
