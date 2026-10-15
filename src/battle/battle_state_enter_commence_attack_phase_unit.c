#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/*
 * Copies the caster's action target data (unit_stats_t 0x16e..0x181) into
 * its runtime command state's action, then latches the used ability id.
 */
void battle_state_enter_commence_attack_phase(void) {
    unit_stats_t* stats;
    battle_unit_t* unit;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_COMMENCE_ATTACK_PHASE;
    unit = battle_unit_get_source();
    stats = unit->stats;
    unit->command_state.ai.data.action = stats->action_command;
    /* action_command.ability_id is s16 in the header; the target loads it with lhu. */
    unit->used_ability_id = unit->stats->action_command.ability_id;
    unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
    if (unit->used_ability_id != 0) {
        battle_unit_start_ability_charge_hold_animation(unit);
    }
}

/* Enter the ability-preview-help state and start the help menu. Nothing in the
 * game calls it. */
void battle_menu_start_help_thread(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HELP;
    g_battle_menu_help_opening = 1;
    menu_start_building_thread();
}

/*
 * Seed the source unit's pending action target and run target-tile selection.
 *
 * During ability-preview phase 2 the target is the unit's own tile; for
 * player-controlled units it is the current cursor tile.
 * target_calculate_for_menu_types's result
 * picks the next step: 0/1 enter the ability-preview state, 2 restores the
 * unit and stops game flow, and -1 returns to the idle action menu.
 */
void target_select_tile(void) {
    battle_unit_t* unit;
    s32 result;
    s32 prev;

    unit = battle_unit_get_source();
    unit->state_frame_counter = 0;
    if (unit->ability_preview_phase == TARGET_PANELS_NO_TARGETING) {
        main_util_set_svector(
            &unit->command_state.ai.data.action.target_position, unit->map_x, unit->map_z, unit->map_y);
        unit->command_state.ai.data.action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
    } else if (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
        main_util_set_svector(&unit->command_state.ai.data.action.target_position, g_battle_cursor_x, g_battle_cursor_z,
            g_battle_cursor_y);
        unit->command_state.ai.data.action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
    }
    result = target_calculate_for_menu_types(&unit->command_state.ai.data.action);
    unit->target_select_result = result;
    switch (result) {
    case 0:
    case 1:
        camera_enable_map_pan();
        prev = g_battle_cursor_input_mode;
        g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING;
        g_battle_cursor_input_mode = BATTLE_CURSOR_INPUT_FREE_CURSOR;
        g_battle_cursor_input_mode_saved = prev;
        target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_TARGETS, 3);
        target_store_cursor_casting_unit_name_and_data();
        if (g_main_game_options.fields.target_flashing == GAME_OPTION_ON) {
            battle_gfx_tint_all_units_by_team();
        }
        break;
    case 2:
        camera_disable_map_pan();
        battle_state_enter_pre_attack_animation();
        target_store_cursor_casting_unit_name_and_data();
        break;
    case -1:
        battle_menu_dispatch_idle_action_menu();
        break;
    }
    action_clear_at_list_active();
}

/* Enter the illegal-range state, reached when the chosen tile lacks the
 * ability-range mark: opens action-confirm option 5 for the source unit. */
void battle_state_enter_target_out_of_range(void) {
    battle_unit_t* unit;
    unit_stats_t* stats;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ILLEGAL_RANGE;
    unit = battle_unit_get_source();
    stats = unit->stats;
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 5, stats->battle_id, 0,
        unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Start target selection for the source unit's previewed action and return its
 * ability_preview_phase (target_panels_e).
 *
 * An action with a target to choose enters the targeting-range state with the
 * range tiles shown and the cursor unit as preview target. An action without
 * targeting selects its tile at once. With no valid panels the idle action menu
 * reopens. */
s32 target_set_boxes_red(void) {
    battle_unit_t* unit;

    unit = battle_unit_get_source();
    unit->state_frame_counter = 0;
    switch (unit->ability_preview_phase) {
    case TARGET_PANELS_UNIT_OR_TILE:
    case TARGET_PANELS_TILE_ONLY:
        camera_disable_map_pan();
        g_battle_game_state = BATTLE_GAME_STATE_TARGETING_RANGE;
        target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_RANGE, 2);
        target_store_cursor_unit_as_preview_target();
        if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
            action_set_at_list_active();
        } else {
            action_clear_at_list_active();
        }
        break;
    case TARGET_PANELS_NO_TARGETING:
        camera_disable_map_pan();
        target_select_tile();
        target_store_cursor_unit_as_preview_target();
        break;
    case TARGET_PANELS_INVALID:
    case TARGET_PANELS_NONE:
        battle_menu_dispatch_idle_action_menu();
        break;
    }
    return unit->ability_preview_phase;
}
