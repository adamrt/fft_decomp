#include "fft/event_card.h"

void card_input_update_suppressed_during_fade(void) {
    card_input_update_controller();
    if (gfx_get_fade_state() != 0) {
        g_menu_input_newly_pressed = 0;
        g_menu_input_primary_repeat = 0;
        g_menu_input_secondary_repeat = 0;
    }
}
