#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "psx/types.h"

/* TargetDisplayStart game-state handler: once the prompt has closed (command 7, 8
 * or 0xff) and the camera has settled, enter the target display. */
void battle_state_handle_target_display_start_state(void) {
    s32 id;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    id = menu_get_selected_command_address()->kind;
    if ((id >= 7) && ((id < 9) || (id == 0xFF))) {
        g_action_post_action = 1;
    }
    if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0) {
        if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
            if (g_action_post_action != 0) {
                battle_state_enter_target_display();
            }
        }
    }
}
