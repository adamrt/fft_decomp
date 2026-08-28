#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void battle_state_handle_unit_moving_setup_state(void) {
    s32* selected_ability_address;
    s32 selected_ability;
    battle_unit_misc_data_t* casting_misc_data;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    selected_ability_address = battle_menu_get_selected_ability_address();
    casting_misc_data = battle_unit_get_casting_misc_data();
    selected_ability = *selected_ability_address;
    switch (selected_ability) {
    case 7:
        battle_state_enter_unit_moving();
        return;
    case 8:
    case 0xff:
        casting_misc_data->movement_path_count = 0;
        battle_menu_close_move_help();
        return;
    }
}
