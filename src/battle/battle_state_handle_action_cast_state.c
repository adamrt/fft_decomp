#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "psx/types.h"

/* ActionCast game-state handler: once the message has closed and the camera has
 * settled, enter the commence-attack phase. */
void battle_state_handle_action_cast_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    /* Selections 7, 8 and 0xff are the ones ACTION_EXECUTE_SETUP handles;
     * they mark the action as finished (same test as the
     * BATTLE_MESSAGE_DISPLAY handler). */
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        g_action_post_action = 1;
    }
    /* Wait for the camera to settle before handing the action data over. */
    if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_action_post_action != 0) {
        battle_state_enter_commence_attack_phase();
    }
}
