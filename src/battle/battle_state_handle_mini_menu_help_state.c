#include "fft/battle.h"

/* MiniMenuHelp game-state handler: when the help window has gone, reopen the mini
 * menu. */
void battle_state_handle_mini_menu_help_state(void) {
    if (menu_get_thread_activity_variant() != 2) {
        g_battle_menu_help_opening = 0;
        battle_menu_open_free_cursor_mini_menu();
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
}
