#include "fft/battle.h"

/* AfterCommand game-state handler: once the window has closed (command 7, 8 or
 * 0xff) and the source unit's numeric display is idle, enter the JP/EXP gain
 * state. */
void battle_state_handle_after_command_state(void) {
    ai_command_t* command_address;
    s32 command;
    battle_unit_t* unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    command_address = menu_get_selected_command_address();
    unit = battle_unit_get_source();
    command = command_address->kind;
    if ((command >= 7) && ((command < 9) || (command == 0xFF))) {
        g_action_post_action = 1;
    }
    if ((g_action_post_action != 0) && (unit->numeric_display.active == 0)) {
        battle_state_enter_jp_xp_gain();
    }
}
