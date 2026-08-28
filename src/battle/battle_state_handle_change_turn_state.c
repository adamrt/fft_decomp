#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void battle_state_handle_change_turn_state(void) {
    s32 ability;
    s32 type;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    ability = *battle_menu_get_selected_ability_address();
    if (ability >= 7) {
        if (ability < 9) {
            g_battle_action_post_action = 1;
        } else if (ability == 0xFF) {
            g_battle_action_post_action = 1;
        }
    }
    if (g_battle_action_post_action != 0) {
        if ((g_battle_current_vector.vx | g_battle_current_vector.vy | g_battle_current_vector.vz) == 0) {
            if (g_battle_camera_rotation_action == 0) {
                battle_unit_get_source_misc_data();
                type = g_action_type;
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
