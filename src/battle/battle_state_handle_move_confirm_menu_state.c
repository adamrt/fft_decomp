#include "fft/battle.h"
#include "psx/types.h"

void battle_state_handle_move_confirm_menu_state(void) {
    s32* selected_ability_address;
    s32 selected_ability;
    battle_unit_misc_data_t* source_misc_data;
    battle_unit_misc_data_t* casting_misc_data;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    selected_ability_address = battle_menu_get_selected_ability_address();
    source_misc_data = battle_unit_get_source_misc_data();
    casting_misc_data = battle_unit_get_casting_misc_data();
    selected_ability = *selected_ability_address;
    switch (selected_ability) {
    case 8:
    case 0xff:
        if (casting_misc_data->movement_flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT) {
        case 7:
            battle_unit_copy_rider_data_to_mount(source_misc_data, casting_misc_data);
            return;
        }
        battle_move_undo_unit_move(casting_misc_data);
        battle_gfx_update_unit_palettes();
        battle_menu_close_move_help();
        return;
    }
}
