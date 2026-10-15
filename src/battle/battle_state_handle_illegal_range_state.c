#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_target.h"
#include "psx/pad.h"

/* IllegalRange game-state handler: when the message closes (command 7, 8 or
 * 0xff), return to the range display. */
void battle_state_handle_illegal_range_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        target_set_boxes_red();
    }
}
