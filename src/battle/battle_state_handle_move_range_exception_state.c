#include "fft/battle.h"

/* MoveRangeException game-state handler: when the message closes (command 7, 8 or
 * 0xff), reopen the idle action menu of the unit under the cursor. */
void battle_state_handle_move_range_exception_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    battle_menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        battle_menu_dispatch_idle_action_menu();
    }
}
