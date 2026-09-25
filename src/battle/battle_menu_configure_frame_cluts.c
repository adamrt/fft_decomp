#include "fft/battle.h"
#include "fft/battle_menu_window.h"

/* Select the menu-frame palette rows for the active thread context. */
void battle_menu_configure_frame_cluts(battle_menu_window_record_t* frame) {
    if (battle_thread_is_previous_running() != 0 || g_event_mode == 1) {
        frame->sprites[0].clut = 0x7d3c;
        frame->sprites[1].clut = 0x7dfc;
        frame->sprites[2].clut = 0x7e3c;
        frame->sprites[3].clut = 0x7c7c;
    } else {
        frame->sprites[0].clut = 0x7c3c;
        frame->sprites[1].clut = 0x7d7c;
        frame->sprites[2].clut = 0x7dbc;
        frame->sprites[3].clut = 0x7cbc;
    }
}
