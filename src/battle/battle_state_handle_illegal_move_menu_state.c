#include "fft/battle.h"

/* IllegalMoveMenu game-state handler: when the message closes (command 7, 8 or
 * 0xff), return to choosing a destination (battle_menu_close_move_help). */
void battle_state_handle_illegal_move_menu_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        battle_menu_close_move_help();
    }
}
