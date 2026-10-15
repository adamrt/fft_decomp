#include "fft/event_bunit.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Per-frame input update of the BUNIT screen.
 *
 * Reads the pad, then clears the 32-bit input values while the text thread runs or a
 * help screen is open, on the frame the text ends and during a screen fade.
 * The help text state goes 1 on the text thread's first frame (which queues
 * the window-open sound), 2 after it and 0 otherwise.
 */
void bunit_menu_update_event_thread_state(void) {
    s32 running;

    bunit_input_update_controller();
    running = thread_is_running(THREAD_SCREEN_SLOT_TEXT);

    if (running != 0 || g_bunit_help_screen_id != MENU_GRID_HELP_NONE) {
        menu_input_clear_state();
        if (running != 0) {
            g_bunit_help_text_state = (g_bunit_help_text_state != 0) ? 2 : 1;
        } else {
            g_bunit_help_text_state = 0;
        }
    } else if (g_bunit_help_text_state != 0 || bunit_gfx_get_fade_state() != 0) {
        g_bunit_help_text_state = 0;
        menu_input_clear_state();
    }

    if (running == 0) {
        g_menu_input_lock_mode = 0;
    }
    if (g_bunit_help_text_state == 1) {
        g_bunit_sound_queued_effect_id = MAIN_SFX_WINDOW_OPEN;
    }
}
