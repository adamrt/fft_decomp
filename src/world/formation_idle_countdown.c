/* The idle countdown and the input lockout timer. */

#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/thread.h"
#include "fft/world_script.h"
#include "psx/types.h"

/* Count g_world_input_lockout_timer down while thread 7 (the comparison status panel) is idle
 * and no status panel slides, and reset it to 10 otherwise. While it is nonzero, the menu input
 * masks are cleared (menu_input_clear_state). */
void world_input_update_lockout_timer(void) {
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

/* Countdown maintained by world_menu_update_thread_7_idle_countdown; while it is non-zero the formation
   screen's per-frame handler returns before reading controller input. */
s32 world_input_get_lockout_timer(void) {
    return g_world_input_lockout_timer;
}
