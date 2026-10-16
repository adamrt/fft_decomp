/* The idle countdown and the input lockout timer. */

#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/screen.h"
#include "fft/thread.h"
#include "fft/unit_panel.h"
#include "psx/types.h"

/* Count g_screen_input_lockout_timer down while thread 7 (the comparison status panel) is idle
 * and no status panel slides, and reset it to 10 otherwise. While it is nonzero, the menu input
 * masks are cleared (menu_input_clear_state). */
void screen_input_update_lockout(void) {
    s32 value;

    if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) != 0) {
        value = 10;
    } else if (g_unit_panel_status_display_thread_params.flags
        & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP)) {
        value = 10;
    } else {
        if (g_screen_input_lockout_timer == 0) {
            return;
        }
        value = g_screen_input_lockout_timer - 1;
    }
    g_screen_input_lockout_timer = value;
    if (g_screen_input_lockout_timer != 0) {
        menu_input_clear_state();
    }
}

/* Countdown maintained by screen_input_update_lockout; while it is nonzero the formation
 * screen's per-frame handler returns before reading controller input. */
s32 screen_input_get_lockout(void) {
    return g_screen_input_lockout_timer;
}
