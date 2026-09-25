#include "fft/battle.h"
#include "psx/types.h"

void battle_state_handle_wait_menu_state(void) {
    s32* selected_ability_address;
    s32 selected_ability;
    battle_unit_misc_data_t* source_misc_data;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    selected_ability_address = battle_menu_get_selected_ability_address();
    source_misc_data = battle_unit_get_source_misc_data();
    selected_ability = *selected_ability_address;
    switch (selected_ability) {
    case 8:
    case 0xff:
        if (battle_action_check_change_of_turn(source_misc_data->battle_data->misc_unit_id) != 1) {
            battle_state_enter_after_command();
            return;
        }
    case 7:
        battle_action_choose_facing_for_wait();
        return;
    }
}
