#include "fft/battle.h"
#include "psx/types.h"

void battle_state_handle_action_cast_state(void) {
    s32 selected_ability;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    selected_ability = *battle_menu_get_selected_ability_address();
    /* Selections 7, 8 and 0xff are the ones ACTION_EXECUTE_SETUP handles;
     * they mark the action as finished (same test as the
     * BATTLE_MESSAGE_DISPLAY handler). */
    if (selected_ability >= 7 && (selected_ability < 9 || selected_ability == 0xff)) {
        g_battle_action_post_action = 1;
    }
    /* Wait for the camera to settle before handing the action data over. */
    if ((g_battle_current_vector.vx | g_battle_current_vector.vy | g_battle_current_vector.vz) == 0
        && g_battle_camera_rotation_action == 0 && g_battle_action_post_action != 0) {
        battle_state_enter_commence_attack_phase();
    }
}
