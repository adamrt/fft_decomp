#include "fft/battle.h"
#include "psx/types.h"

/* ActionAnnouncement game-state handler: when the announcement closes (command 7,
 * 8 or 0xff), go on to load the casting unit's SP2 sheet. */
void battle_state_handle_action_announcement_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        battle_state_enter_open_sp2_files();
    }
}
