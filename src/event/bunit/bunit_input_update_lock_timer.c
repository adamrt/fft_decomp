#include "fft/event_bunit.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Hold the menu input masks at zero while the comparison banner thread runs,
 * a status banner slide is requested or the unit list has to scroll, and for
 * nine further calls after the last of those. */
void bunit_input_update_lock_timer(void) {
    s32 timer;

    if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) != 0) {
        timer = 0xA;
    } else if ((g_bunit_status_display_flags & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP)) != 0) {
        timer = 0xA;
    } else if (bunit_menu_is_unit_list_scroll_needed() != 0) {
        timer = 0xA;
    } else {
        timer = g_bunit_input_lock_timer;
        if (timer == 0) {
            return;
        }
        timer -= 1;
    }
    g_bunit_input_lock_timer = timer;
    if (g_bunit_input_lock_timer != 0) {
        g_menu_input_newly_pressed = 0;
        g_menu_input_secondary_repeat = 0;
        g_menu_input_primary_repeat = 0;
    }
}
