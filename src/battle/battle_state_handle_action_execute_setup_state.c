#include "fft/battle.h"
#include "fft/battle_target.h"

/* ActionExecuteSetup game-state handler: command 7 goes on to the range display
 * (target_set_boxes_red); 8 or 0xff returns to the idle action menu. */
void battle_state_handle_action_execute_setup_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    switch (selected_command) {
    case 7:
        target_set_boxes_red();
        return;
    case 8:
    case 0xff:
        battle_menu_open_active_unit_idle_action_menu();
        return;
    }
}
