#include "fft/bunit.h"
#include "psx/types.h"

void bunit_input_update_lock_timer(void) {
    s32 timer;

    if (battle_thread_is_running(7) != 0) {
        timer = 0xA;
    } else if ((g_bunit_status_display_flags & 0x60) != 0) {
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
        g_bunit_menu_input_mask = 0;
        g_bunit_menu_input_repeat_mask = 0;
        g_bunit_menu_input_active_mask = 0;
    }
}
