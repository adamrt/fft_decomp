#include "fft/battle.h"
#include "psx/types.h"

/* WaitMenu game-state handler. Command 7 goes on to choosing a facing. Command 8
 * or 0xff returns to the after-command state, unless the unit's turn is over
 * (action_check_change_of_turn returns 1): then it chooses a facing too. */
void battle_state_handle_wait_menu_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    battle_unit_t* source_unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    source_unit = battle_unit_get_source();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case 8:
    case 0xff:
        if (action_check_change_of_turn(source_unit->stats->battle_id) != 1) {
            battle_state_enter_after_command();
            return;
        }
    case 7:
        action_choose_facing_for_wait();
        return;
    }
}
