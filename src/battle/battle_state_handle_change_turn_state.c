#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "psx/types.h"

/* ChangeTurn game-state handler. Once the menu layer reports command 7, 8 or 0xff
 * (the message closed) and the camera has settled (no cursor vector, no
 * rotation), the turn event type picks the next state: a ready ability enters the
 * action cast, an action result or a Mime action enters status execution, and
 * anything else opens the next action menus. */
void battle_state_handle_change_turn_state(void) {
    s32 command;
    s32 type;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    command = menu_get_selected_command_address()->kind;
    if (command >= 7) {
        if (command < 9) {
            g_action_post_action = 1;
        } else if (command == 0xFF) {
            g_action_post_action = 1;
        }
    }
    if (g_action_post_action != 0) {
        if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0) {
            if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
                battle_unit_get_source();
                type = g_action_turn_event_type;
                if (type == BATTLE_TURN_EVENT_ABILITY_READY) {
                    battle_state_enter_action_cast();
                    return;
                }
                if ((type == BATTLE_TURN_EVENT_ACTION_RESULT) || (type == BATTLE_TURN_EVENT_MIME)) {
                    battle_state_enter_status_execute();
                    return;
                }
                battle_menu_set_next_script_action_menus();
            }
        }
    }
}
