#include "fft/battle.h"

void battle_state_handle_unit_move_state(void) {
    s32 selected_ability;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    selected_ability = *battle_menu_get_selected_ability_address();
    switch (selected_ability) {
    case 7:
        battle_menu_close_move_help();
        return;
    case 8:
    case 0xff:
        battle_menu_open_active_unit_idle_action_menu();
        return;
    }
}
