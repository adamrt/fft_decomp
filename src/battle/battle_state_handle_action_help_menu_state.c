#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"

void battle_state_handle_action_help_menu_state(void) {
    battle_unit_misc_data_t* misc_data;

    if (battle_menu_is_still_building() != 2) {
        g_battle_menu_help_opening = 0;
        misc_data = battle_unit_get_casting_misc_data();
        if (misc_data != 0) {
            if (misc_data->unit_id == g_casting_unit_misc_id) {
                battle_menu_open_active_unit_idle_action_menu();
            } else {
                battle_menu_open_non_active_unit_idle_action_menu();
            }
        }
    }
    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
}
