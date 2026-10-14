#include "fft/battle.h"

/* UnitMove game-state handler: command 7 goes on to choosing a destination
 * (battle_menu_close_move_help); 8 or 0xff returns to the idle action menu. */
void battle_state_handle_unit_move_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    switch (selected_command) {
    case 7:
        battle_menu_close_move_help();
        return;
    case 8:
    case 0xff:
        battle_menu_open_active_unit_idle_action_menu();
        return;
    }
}
