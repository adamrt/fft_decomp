/* The idle countdown and the input lockout timer. */

#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world_script.h"
#include "psx/types.h"

/* Count the timer at g_world_input_lockout_timer down while thread 7 is idle; reset it to 10
 * otherwise, and clear the 0x801cd000 area while it is still running. */
void world_menu_update_thread_7_idle_countdown(void) {
    s32 value;

    if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) != 0) {
        value = 10;
    } else if (g_menu_status_display_thread_params.flags
        & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP)) {
        value = 10;
    } else {
        if (g_world_input_lockout_timer == 0) {
            return;
        }
        value = g_world_input_lockout_timer - 1;
    }
    g_world_input_lockout_timer = value;
    if (g_world_input_lockout_timer != 0) {
        menu_input_clear_state();
    }
}

/* Countdown maintained by 0x80113580; while it is non-zero the formation
   screen's per-frame handler returns before reading controller input. */
s32 world_input_get_lockout_timer(void) {
    return g_world_input_lockout_timer;
}
