#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/*
 * Seed the source unit's pending action target and run target-tile selection.
 *
 * During ability-preview phase 2 the target is the unit's own tile; for
 * player-controlled units it is the current cursor tile.
 * battle_target_calculate_for_menu_types's result
 * picks the next step: 0/1 enter the ability-preview state, 2 restores the
 * unit and stops game flow, and -1 returns to the idle action menu.
 */
void battle_target_select_tile(void) {
    battle_unit_misc_data_t* unit;
    s32 result;
    s32 prev;

    unit = battle_unit_get_source_misc_data();
    unit->state_frame_counter = 0;
    if (unit->ability_preview_phase == 2) {
        main_util_set_svector(
            (SVECTOR*)&unit->command_state.ai.data.action.target_x, unit->map_x, unit->map_z, unit->map_y);
        unit->command_state.ai.data.action.targeting_type = 5;
    } else if (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
        main_util_set_svector((SVECTOR*)&unit->command_state.ai.data.action.target_x, g_battle_cursor_x,
            g_battle_cursor_z, g_battle_cursor_y);
        unit->command_state.ai.data.action.targeting_type = 5;
    }
    result = battle_target_calculate_for_menu_types(&unit->command_state.ai.data.action.unit_id);
    unit->target_select_result = result;
    switch (result) {
    case 0:
    case 1:
        battle_state_start_game_flow();
        prev = g_battle_controller_input;
        g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING;
        g_battle_controller_input = 2;
        g_controller_input_copy_12 = prev;
        battle_target_set_tile_background_color(7, 3);
        battle_target_store_cursor_casting_unit_name_and_data();
        if (g_main_game_options.fields.target_flashing == GAME_OPTION_ON) {
            battle_gfx_tint_all_units_by_team();
        }
        break;
    case 2:
        battle_state_stop_game_flow();
        battle_state_enter_pre_attack_animation();
        battle_target_store_cursor_casting_unit_name_and_data();
        break;
    case -1:
        battle_menu_dispatch_idle_action_menu();
        break;
    }
    battle_action_clear_at_list_active();
}
