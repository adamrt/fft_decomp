#include "fft/world.h"

/* Tutorial command: count the wait timer down by the VSync mode and move to
 * the next command when it reaches zero. */
void world_script_handle_tutorial_command_wait(void) {
    g_world_script_tutorial_wait_timer -= menu_get_event_speed();
    if (g_world_script_tutorial_wait_timer <= 0) {
        g_world_script_tutorial_wait_timer = 0;
        g_world_script_tutorial_command_ptr++;
    }
}
