#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"

void battle_state_handle_confirm_action_state(void) {
    /* Pin: unpinned (either declaration order) GCC puts the selected-ability
     * pointer in $s0 and the unit in $s1, the reverse of the target. */
    register battle_unit_misc_data_t* unit asm("$16");
    s32* selected_ability_address;
    battle_unit_misc_data_t* target;
    s32 selected_ability;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    selected_ability_address = battle_menu_get_selected_ability_address();
    unit = battle_unit_get_source_misc_data();
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        selected_ability = *selected_ability_address;
        switch (selected_ability) {
        case 5:
            unit->command_state.ai.data.action.targeting_type = *selected_ability_address;
            main_util_set_svector(
                &unit->command_state.cursor.target_panel, g_battle_cursor_x, g_battle_cursor_z, g_battle_cursor_y);
            battle_state_enter_pre_attack_animation();
            return;
        case 6:
        case 7:
            target = battle_unit_get_selectable_misc_data_at_map_coords(
                g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
            if (target != 0) {
                unit->command_state.ai.data.action.targeting_type = *selected_ability_address;
                unit->command_state.ai.data.action.target_id = target->battle_data->misc_unit_id;
            } else {
                unit->command_state.ai.data.action.targeting_type = 5;
            }
            main_util_set_svector(
                &unit->command_state.cursor.target_panel, g_battle_cursor_x, g_battle_cursor_z, g_battle_cursor_y);
            battle_state_enter_pre_attack_animation();
            return;
        case 8:
        case 0xff:
            battle_target_set_tile_background_color(0, 3);
            battle_target_set_boxes_red();
            return;
        default:
            break;
        }
    } else {
        selected_ability = *selected_ability_address;
        switch (selected_ability) {
        case 5:
        case 6:
        case 7:
            battle_state_enter_pre_attack_animation();
            return;
        case 8:
        case 0xff:
            battle_target_set_tile_background_color(0, 3);
            battle_target_set_boxes_red();
            return;
        default:
            break;
        }
    }
}
