#include "fft/battle.h"

/* FreeCursorHelp game-state handler: when the help window has gone, return to the
 * free cursor. */
void battle_state_handle_free_cursor_help_state(void) {
    if (menu_get_thread_activity_variant() != 2) {
        g_battle_menu_help_opening = 0;
        battle_state_set_free_cursor();
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}
