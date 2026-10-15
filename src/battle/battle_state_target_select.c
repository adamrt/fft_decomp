#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/* Enter the confirm-action state and open the confirmation prompt for the pending
 * action.
 *
 * The prompt option follows the ability preview's targeting mode and whether a
 * selectable unit stands on the cursor tile: 1 for a unit under a unit-or-tile
 * ability, 4 for a unit under a tile-only ability, 3 for a unit under an ability
 * with no target choice, 2 for an empty tile in all three, and 0 for any other
 * mode. */
void action_confirm(void) {
    battle_unit_t* unit;
    battle_unit_t* target;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_CONFIRM_ACTION;
    unit = battle_unit_get_source();
    target = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    switch (unit->ability_preview_phase) {
    case TARGET_PANELS_UNIT_OR_TILE:
        if (target != 0) {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 1, unit->stats->battle_id, 0,
                unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 2, unit->stats->battle_id, 0,
                unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        }
        break;
    case TARGET_PANELS_TILE_ONLY:
        if (target != 0) {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 4, unit->stats->battle_id, 0,
                unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 2, unit->stats->battle_id, 0,
                unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        }
        break;
    case TARGET_PANELS_NO_TARGETING:
        if (target != 0) {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 3, unit->stats->battle_id, 0,
                unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 2, unit->stats->battle_id, 0,
                unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        }
        break;
    default:
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 0, unit->stats->battle_id, 0,
            unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        break;
    }
}

/* Starts the repeating tint that marks the candidates of an auto-battle target
 * choice: red on every unit of another team than the casting unit's for
 * AI_COMMAND_AUTOBATTLE_TARGETED_SELECT, blue on every unit of its own
 * team for AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT. Other modes tint
 * nothing. */
void battle_gfx_highlight_all_units_blue_or_red(s32 mode) {
    battle_unit_t* caster;
    battle_unit_t* unit;
    s32 i;

    caster = battle_unit_get_casting();
    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        unit = battle_unit_get_by_runtime_id(i);
        if (unit == 0) {
            continue;
        }
        if (unit->stats == 0) {
            continue;
        }
        switch (mode) {
        case AI_COMMAND_AUTOBATTLE_TARGETED_SELECT:
            if ((unit->stats->team_flags & BATTLE_TEAM_MASK) != (caster->stats->team_flags & BATTLE_TEAM_MASK)) {
                map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, 4, MAP_CLUT_ROW_UNITS, i, 0, 0x10, 0, 0);
            }
            continue;
        case AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT:
            if ((unit->stats->team_flags & BATTLE_TEAM_MASK) == (caster->stats->team_flags & BATTLE_TEAM_MASK)) {
                map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, 4, MAP_CLUT_ROW_UNITS, i, 0, 0, 0, 0x18);
            }
            continue;
        }
    }
}

/* Enter the target-select-start state: open the targeting prompt for the casting
 * unit. */
void battle_state_enter_target_select_start(void) {
    battle_unit_t* unit;
    unit_stats_t* stats;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT_START;
    unit = battle_unit_get_casting();
    stats = unit->stats;
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 0, stats->battle_id, 0, 1);
    action_set_at_list_active();
}

/* Enter the target-select state: show the panel for the cursor tile and tint the
 * units the casting unit's selection command may pick, the other teams' red for a
 * targeted selection and its own team's blue for a recovery selection. */
void battle_state_enter_target_select(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT;
    target_store_cursor_unit_name_and_data();
    battle_gfx_highlight_all_units_blue_or_red(battle_unit_get_casting()->target_select_command);
}

/* Enter the target-select-denied state for the cursor tile: show the summary of a
 * unit standing there and open the refusal variant of the target-confirm prompt
 * for the casting unit.
 *
 * Option 3 is used for an empty tile, 1 for a targeted auto-battle selection and
 * 2 for a recovery selection. Any other selection command over a unit opens no
 * prompt. */
void battle_state_enter_target_select_denied(void) {
    battle_unit_t* runtime;
    battle_unit_t* unit;
    unit_stats_t* stats;
    s32 kind;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT_DENIED;
    runtime = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (runtime != 0) {
        stats = runtime->stats;
        if (stats != 0) {
            menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, stats->battle_id, 0);
        }
    }
    unit = battle_unit_get_casting();
    if (runtime == 0) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 3, unit->stats->battle_id, 0, 1);
        return;
    }
    kind = unit->target_select_command;
    if (kind == AI_COMMAND_AUTOBATTLE_TARGETED_SELECT) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 1, unit->stats->battle_id, 0, 1);
        return;
    }
    if (kind == AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 2, unit->stats->battle_id, 0, 1);
    }
}

/* Enter the target-select-confirm state: show the summary of the unit under the
 * cursor and open the target-confirm prompt (option 0) for the casting unit. */
void battle_state_enter_target_select_confirm(void) {
    battle_unit_t* unit;
    unit_stats_t* stats;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT_CONFIRM;
    unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (unit != 0) {
        stats = unit->stats;
        if (stats != 0) {
            menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, stats->battle_id, 0);
        }
    }
    stats = battle_unit_get_casting()->stats;
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 0, stats->battle_id, 0, 1);
}

/* Enter the target-display-start state: show the tile info panel and open the
 * targeting prompt for the casting unit. */
void battle_state_enter_target_display_start(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_DISPLAY_START;
    menu_start_unit_view_thread(UNIT_VIEW_MODE_TILE_INFO, 0xFF, 0xFF);
    menu_init_system_function(
        BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 0, battle_unit_get_casting()->stats->battle_id, 0, 1);
    g_action_post_action = 0;
    action_set_at_list_active();
}

/* Enter the target-display state: move the cursor to the casting unit's
 * auto-battle target, turn the camera when its tile is hidden and show its
 * summary panel. */
void battle_state_enter_target_display(void) {
    unit_stats_t* cast;
    battle_unit_t* unit;
    unit_stats_t* target;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_DISPLAY;
    cast = battle_unit_get_casting()->stats;
    unit = battle_unit_get_by_battle_id(cast->auto_battle_target);
    target_move_cursor_to_unit(unit);
    camera_rotate_when_unit_tile_not_visible(unit);
    if (unit != 0) {
        target = unit->stats;
        if (target != 0) {
            menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, target->battle_id, 0);
        }
    }
}

/* Enter the Effect game state at two vertical blanks per frame. No code calls it
 * or battle_state_enter_effect_playback, the only other entry to that state. */
void battle_state_halve_animation_speed_for_effects(void) {
    g_main_system_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_EFFECT;
    action_set_at_list_active();
}

/* Enter the MapJumpingOut game state at two vertical blanks per frame. No code
 * calls it. */
void battle_state_halve_animation_speed_for_jump_out(void) {
    g_main_system_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_OUT;
}

/* Enter the MapJumpingIn game state at two vertical blanks per frame. No code
 * calls it. */
void battle_state_halve_animation_speed_for_jump_in(void) {
    g_main_system_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_IN;
}

/* Queue the CloseBattle game state; callers always pass transition step 8. */
void battle_state_halve_animation_speed_and_queue_close(s32 transition_step, s32 close_flow_state) {
    g_main_system_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_CLOSE_BATTLE;
    g_battle_state_screen_fade_step = transition_step;
    g_main_system_game_flow_state = close_flow_state;
}
