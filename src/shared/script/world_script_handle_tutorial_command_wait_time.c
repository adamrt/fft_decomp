#include "fft/world.h"

/* Tutorial command: wait the number of frames in the command's operand byte
 * (counted by the VSync mode), then step past the two-byte command. */
/* Address-of operand fields preserves the -O0 byte-address steps. */
void world_script_handle_tutorial_command_wait_time(void) {
    if (g_world_script_tutorial_command_active != 0) {
        g_world_script_tutorial_wait_time_remaining -= menu_get_event_speed();
        if (g_world_script_tutorial_wait_time_remaining < 0) {
            g_world_script_tutorial_command_active = 0;
            g_world_script_tutorial_command_ptr += 2;
        }
    } else {
        g_world_script_tutorial_command_active = 1;
        g_world_script_tutorial_wait_time_remaining
            = *(&((world_tutorial_wait_instruction_t*)g_world_script_tutorial_command_ptr)->frames);
    }
}
