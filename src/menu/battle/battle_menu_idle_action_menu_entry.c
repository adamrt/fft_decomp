/* Idle action menu openers with the status-execute, highlight and help entries. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

void battle_state_enter_status_execute(void) {
    battle_unit_t* unit;
    u32 statuses;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_STATUS_EXECUTE;
    unit = battle_unit_get_source();
    action_clear_at_list_active();
    g_action_post_action = 0;
    if (g_action_turn_event_type == BATTLE_TURN_EVENT_MIME) {
        text_set_message_duration_frames(0x3C);
        menu_init_system_function(
            BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x183A, unit->stats->battle_id, unit->stats->battle_id, 1);
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_KNEEL, unit->facing, unit);
        return;
    }
    text_set_message_duration_frames(0xB4);
    statuses = unit->statuses_to_add_5_6;
    if (statuses & BATTLE_UNIT_STATUS_CRYSTAL) {
        menu_init_system_function(
            BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x183D, unit->stats->battle_id, unit->stats->battle_id, 1);
    } else if (statuses & BATTLE_UNIT_STATUS_TREASURE) {
        menu_init_system_function(
            BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x183C, unit->stats->battle_id, unit->stats->battle_id, 1);
    } else {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
    battle_unit_update_display_by_runtime_id(unit->runtime_id);
    battle_gfx_prepare_post_action_display_by_runtime_id(unit->runtime_id);
}

/* Enter the menu-to-targeting state and restart the source unit's command. */
void battle_state_restart_menu_to_targeting(void) {
    battle_unit_t* unit;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MENU_TO_TARGETING;
    unit = battle_unit_get_source();
    unit->state_frame_counter = 0;
    unit->command_ready = 0;
    target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 0);
    if (unit != 0 && unit->stats != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, unit->stats->battle_id, 0);
    }
}

/* Enter the highlight-units state: tint every unit by its team, save the cursor
 * input mode and switch to the free cursor with map panning. */
void battle_state_enter_highlight_units_by_team(void) {
    camera_enable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_HIGHLIGHT_UNITS;
    g_battle_cursor_input_mode_saved = g_battle_cursor_input_mode;
    g_battle_cursor_input_mode = BATTLE_CURSOR_INPUT_FREE_CURSOR;
    battle_gfx_tint_all_units_by_team();
    action_clear_at_list_active();
}

/* Open the source unit's own action menu: the unit becomes the casting unit, the
 * cursor returns to it, the idling-action-menus state begins with the unit
 * summary panel, and the menu for its remaining move and action is built. */
void battle_menu_open_active_unit_idle_action_menu(void) {
    battle_unit_t* unit;

    camera_disable_map_pan();
    unit = battle_unit_get_source();
    g_battle_casting_runtime_id = unit->runtime_id;
    target_move_cursor_to_unit(unit);
    g_battle_game_state = BATTLE_GAME_STATE_IDLING_ACTION_MENUS;

    if (unit != 0 && unit->stats != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, unit->stats->battle_id, 0);
    }
    menu_build_idle_action_menu_variant(battle_menu_get_id_based_on_mount_moveable_actable(unit->stats->battle_id));
}

/* Opens the non-active-unit idle action menu for the unit under the cursor;
 * g_status_menu_open switches menu ids 0xf/0x10 to 0x11/0x12. */
void battle_menu_open_non_active_unit_idle_action_menu(void) {
    battle_unit_t* runtime;
    unit_stats_t* unit;
    s32 menu_id;

    runtime = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (runtime == 0) {
        return;
    }
    camera_disable_map_pan();
    unit = runtime->stats;
    g_battle_game_state = BATTLE_GAME_STATE_IDLING_ACTION_MENUS;
    menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, unit->battle_id, 0);
    menu_id = battle_menu_get_id_based_on_mount_moveable_actable(runtime->stats->battle_id);
    if (g_status_menu_open != 0) {
        if (menu_id == BATTLE_MENU_ID_PLAYER_UNIT) {
            menu_id = BATTLE_MENU_ID_PLAYER_UNIT_INSPECT;
        } else if (menu_id == BATTLE_MENU_ID_OTHER_UNIT) {
            menu_id = BATTLE_MENU_ID_OTHER_UNIT_INSPECT;
        }
    }
    menu_build_idle_action_menu_variant(menu_id);
}

/* Open the action menu for the unit on the cursor tile, which becomes the casting
 * unit.
 *
 * The source unit, with the status menu closed, gets its own turn menu when the
 * player controls it and otherwise restarts the menu-to-targeting state. Any
 * other unit gets the non-active-unit menu. An empty tile does nothing. */
void battle_menu_dispatch_idle_action_menu(void) {
    battle_unit_t* unit;

    unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (unit == 0) {
        return;
    }
    g_battle_casting_runtime_id = unit->runtime_id;
    if (g_status_menu_open == 0 && unit->runtime_id == g_battle_unit_source_runtime_id) {
        if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
            battle_menu_open_active_unit_idle_action_menu();
        } else {
            battle_state_restart_menu_to_targeting();
        }
    } else {
        battle_menu_open_non_active_unit_idle_action_menu();
    }
}

void battle_menu_open_help(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ACTION_HELP_MENU;
    g_battle_menu_help_opening = 1;
    menu_start_building_thread();
}
