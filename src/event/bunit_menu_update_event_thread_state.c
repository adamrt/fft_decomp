#include "fft/event_bunit.h"
#include "psx/types.h"

void bunit_menu_update_event_thread_state(void) {
    s32 running;

    bunit_input_update_controller();
    running = battle_thread_is_running(1);

    if (running != 0 || g_bunit_help_screen_id != 0) {
        bunit_input_clear_state();
        if (running != 0) {
            g_bunit_help_text_state = (g_bunit_help_text_state != 0) ? 2 : 1;
        } else {
            g_bunit_help_text_state = 0;
        }
    } else if (g_bunit_help_text_state != 0 || bunit_gfx_get_fade_state() != 0) {
        g_bunit_help_text_state = 0;
        bunit_input_clear_state();
    }

    if (running == 0) {
        g_event_mode = 0;
    }
    if (g_bunit_help_text_state == 1) {
        g_bunit_sound_queued_effect_id = MAIN_SFX_WINDOW_OPEN;
    }
}
