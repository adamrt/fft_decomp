#include "fft/world.h"
#include "psx/types.h"

/* Count the timer at g_world_input_lockout_timer down while thread 7 is idle; reset it to 10
 * otherwise, and clear the 0x801cd000 area while it is still running. */
void world_menu_update_thread_7_idle_countdown(void) {
    s32 value;

    if (world_thread_is_running(7) != 0) {
        value = 10;
    } else if (g_world_status_display_thread_params.flags & 0x60) {
        value = 10;
    } else {
        if (g_world_input_lockout_timer == 0) {
            return;
        }
        value = g_world_input_lockout_timer - 1;
    }
    g_world_input_lockout_timer = value;
    if (g_world_input_lockout_timer != 0) {
        world_input_clear_state();
    }
}
