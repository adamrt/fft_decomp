/* Input lockout while the Formation status panels move or compare units. */

#include "fft/menu/menu_input.h"
#include "fft/menu/menu_screen.h"
#include "fft/party/unit_panel.h"
#include "fft/sys/thread.h"
#include "psx/types.h"

/* Countdown maintained by menu_screen_update_input_lockout. */
extern u8 g_menu_screen_input_lockout_timer;

/* Count g_menu_screen_input_lockout_timer down while thread 7 (the comparison status panel) is idle
 * and no status panel slides, and reset it to 10 otherwise. While it is nonzero, the menu input
 * masks are cleared (menu_input_clear_state). Compiled at -O1 with the rest of the Formation
 * screen: the reloads of the timer are that level's code, not a volatile variable. */
void menu_screen_update_input_lockout(void) {
    if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) != 0) {
        g_menu_screen_input_lockout_timer = 10;
    } else if (g_unit_panel_status_display_thread_params.flags
        & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP)) {
        g_menu_screen_input_lockout_timer = 10;
    } else if (g_menu_screen_input_lockout_timer != 0) {
        g_menu_screen_input_lockout_timer--;
    }
    if (g_menu_screen_input_lockout_timer != 0) {
        menu_input_clear_state();
    }
}

/* Countdown maintained by menu_screen_update_input_lockout; while it is nonzero the formation
 * screen's per-frame handler returns before reading controller input. */
s32 menu_screen_get_input_lockout(void) {
    return g_menu_screen_input_lockout_timer;
}
