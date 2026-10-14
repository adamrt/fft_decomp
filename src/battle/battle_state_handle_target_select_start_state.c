#include "fft/battle.h"
#include "psx/types.h"

/* TargetSelectStart game-state handler: when the prompt closes (command 7, 8 or
 * 0xff), enter target selection. */
void battle_state_handle_target_select_start_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    battle_menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        battle_state_enter_target_select();
    }
}
