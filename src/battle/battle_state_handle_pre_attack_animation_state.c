#include "fft/battle.h"

void battle_state_handle_pre_attack_animation_state(void) {
    battle_unit_misc_data_t* unit;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    unit = battle_unit_get_source_misc_data();
    if ((unit->animation_countdown == 0 || unit->secondary_animation_state != 0 || unit->state_frame_counter >= 0x3d)
        && (g_battle_current_vector.vx | g_battle_current_vector.vy | g_battle_current_vector.vz) == 0
        && g_battle_camera_rotation_action == 0) {
        battle_action_execute_ability();
    }
}
