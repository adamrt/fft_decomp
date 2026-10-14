#include "fft/battle.h"
#include "fft/battle_camera.h"

/* PreAttackAnimation game-state handler: execute the ability once the source
 * unit's animation has ended or looped, or its state frame counter has reached
 * 0x3d, and the camera has settled. */
void battle_state_handle_pre_attack_animation_state(void) {
    battle_unit_t* unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    unit = battle_unit_get_source();
    if ((unit->animation.wait == 0 || unit->animation.restart_count != 0 || unit->state_frame_counter >= 0x3d)
        && (g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0
        && g_camera_rotation_action == CAMERA_ROTATION_NONE) {
        action_execute_ability();
    }
}
