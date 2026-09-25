#include "fft/battle.h"
#include "psx/types.h"

void battle_state_handle_target_select_confirm_state(void) {
    battle_unit_misc_data_t* caster;
    battle_unit_misc_data_t* target;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    switch (*battle_menu_get_selected_ability_address()) {
    case 7:
        caster = battle_unit_get_casting_misc_data();
        target = battle_unit_get_selectable_misc_data_at_map_coords(
            g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        battle_menu_store_auto_battle_main_target_id(
            target->battle_data->misc_unit_id, caster->battle_data->misc_unit_id);
        battle_target_move_cursor_to_unit(caster);
        battle_menu_set_next_script_action_menus();
        break;
    case 8:
    case 0xFF:
        battle_state_enter_target_select();
        break;
    }
}
