/* Idle action menus, the free cursor, the move and wait commands, facing changes, and post-action level-up and message
 * reporting. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/main.h"
#include "fft/main_sound.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_overlay.h"
#include "fft/menu_panel.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Hand the battle over to an event script: enter the event state at animation
 * speed 2, reset the jump graphics and the event state, disable the status menu
 * and clear the casting and source unit ids (0xff). */
void battle_action_set_casting_unit_id_ff(void) {
    g_main_system_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_EVENT;
    battle_gfx_reset_jumping_unit_graphic_triggers();
    script_reset_event_state();
    battle_action_clear_at_list_active();
    g_battle_menu_status_enabled = 0;
    g_battle_casting_runtime_id = 0xff;
    g_battle_unit_source_runtime_id = 0xff;
}

/* Enter the event state and run the scenario check with no unit (0xff), as the
 * deployment screen does when it finishes. */
void battle_action_set_casting_unit_id_ff_and_init(void) {
    battle_action_set_casting_unit_id_ff();
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, 0xff, 0, 1);
}

s32 battle_state_try_enter_scenario_event(battle_unit_t* unit) {
    s32 result;

    if (g_battle_action_turn_event_type == BATTLE_TURN_EVENT_UNIT_READY
        || g_battle_action_turn_event_type == BATTLE_TURN_EVENT_NONE) {
        result = menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, unit->stats->battle_id, 0, 1);
    } else if (g_battle_action_turn_event_type == BATTLE_TURN_EVENT_ABILITY_READY
        || g_battle_action_turn_event_type == BATTLE_TURN_EVENT_ACTION_RESULT) {
        result = menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 2, unit->stats->battle_id, 0, 1);
        if (result == MENU_SYSTEM_DISPATCH_EVENT && script_get_event_finish_operation() != 0) {
            if (unit->spritesheet_id < BATTLE_SPRITESHEET_ID_RAMZA_END) {
                result = menu_init_system_function(
                    BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 1, unit->stats->battle_id, 0, 1);
            } else {
                menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, unit->stats->battle_id, 0, 1);
            }
        } else {
            result
                = menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 1, unit->stats->battle_id, 0, 1);
        }
    } else {
        result = menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 1, unit->stats->battle_id, 0, 1);
    }
    if (result == MENU_SYSTEM_DISPATCH_EVENT) {
        g_battle_previous_game_state = g_battle_game_state;
        battle_action_set_casting_unit_id_ff();
        return 1;
    }
    return 0;
}

/* Copy the ENTD control-byte into the unit's runtime data, but strip the
   0x08 (player-controllable) bit when the unit's stats mark it as
   AI-only. Also propagates to the linked mount unit, if any. */
void battle_unit_store_entd_flags_without_control_flag(battle_unit_t* unit) {
    u8 entd_flags;
    u8 mount_entd_flags;
    battle_unit_t* mount;

    entd_flags = unit->stats->team_flags;
    unit->team_flags = entd_flags;
    if (unit->stats->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
        unit->team_flags = entd_flags & ~BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
    }
    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
        mount = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        mount_entd_flags = mount->stats->team_flags;
        mount->team_flags = mount_entd_flags;
        if (mount->stats->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
            mount->team_flags = mount_entd_flags & ~BATTLE_TEAM_FLAG_PLAYER_CONTROLLED;
        }
    }
}

/* Return to the source unit's action menus: set the AT-list-active flag, run the
 * scenario check, and unless it starts an event enter the open-action-menus state
 * and turn the camera when the casting unit's tile is hidden. */
void battle_menu_set_next_script_action_menus(void) {
    battle_unit_t* casting_unit;
    battle_unit_t* unit;

    battle_camera_disable_map_pan();
    casting_unit = battle_unit_get_casting();
    unit = battle_unit_get_source();
    battle_action_set_at_list_active();
    battle_unit_store_entd_flags_without_control_flag(casting_unit);
    if (battle_state_try_enter_scenario_event(unit) == 0) {
        g_battle_game_state = BATTLE_GAME_STATE_OPEN_ACTION_MENUS;
        g_battle_action_post_action = 0;
        battle_camera_rotate_when_unit_tile_not_visible(casting_unit);
    }
}

/* Return to the free-cursor state at animation speed 1: show the panel for the
 * cursor tile and refresh the source unit's control flags from its ENTD flags. */
void battle_state_set_free_cursor(void) {
    battle_unit_t* unit;

    g_main_system_animation_speed = 1;
    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_FREE_CURSOR;
    g_battle_menu_help_opening = 0;
    battle_target_store_cursor_unit_name_and_data();
    battle_action_set_at_list_active();
    unit = battle_unit_get_source();
    if (unit != 0) {
        battle_unit_store_entd_flags_without_control_flag(unit);
    }
}

void battle_menu_open_free_cursor_help(void) {
    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_FREE_CURSOR_HELP;
    g_battle_menu_help_opening = 1;
    menu_start_building_thread();
}

/* Reload the map when the number of crystals on the field has changed.
 *
 * Only in a battle entered with world result 4, and not while such a reload is
 * running: the crystal count, capped at 4, is compared with script variable
 * EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, the map state that GNS rows of
 * those maps select on. A new count is stored and a map data load started; the
 * old count is put back when the load cannot start. Returns whether a reload is
 * in progress. */
static s32 battle_map_check_deep_dungeon_crystal_and_init_state(void) {
    s32 crystal_count;
    s32 previous_count;

    if (g_main_system_frontend_world_result == 4) {
        if (g_battle_map_deep_dungeon_load_in_progress == 0) {
            crystal_count = battle_unit_count_crystals();
            if (crystal_count >= 5) {
                crystal_count = 4;
            }
            previous_count = script_get_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT);
            if (crystal_count != previous_count) {
                script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, crystal_count);
                if (battle_map_start_data_load() != 0) {
                    g_battle_map_deep_dungeon_load_in_progress = 1;
                } else {
                    script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, previous_count);
                }
            }
        }
        return g_battle_map_deep_dungeon_load_in_progress;
    }
    return 0;
}

/* Between turns: when the crystal count calls for a map reload
 * (battle_map_check_deep_dungeon_crystal_and_init_state), enter the Deep Dungeon
 * mesh-load state at animation speed 1. Returns 1 when the state changed. */
static s32 battle_map_try_start_deep_dungeon_mesh_load(void) {
    if (battle_map_check_deep_dungeon_crystal_and_init_state() != 0) {
        g_battle_game_state = BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_LOAD;
        g_main_system_animation_speed = 1;
        return 1;
    }

    return 0;
}

/* As battle_map_try_start_deep_dungeon_mesh_load, entering the mesh-finish state
 * instead: the crystal count check made when a unit's post-movement steps end. */
static s32 battle_map_refresh_deep_dungeon(void) {
    if (battle_map_check_deep_dungeon_crystal_and_init_state()) {
        g_battle_game_state = BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_FINISH;
        g_main_system_animation_speed = 1;
        return 1;
    }
    return 0;
}

/* Resolves the queued in-between-turn event (mimic, ability cast, traps).
 *
 * The 32-bit queue value's second byte selects the action type (g_battle_action_turn_event_type) and its
 * low byte the unit; BATTLE_TURN_EVENT_NONE means nothing is queued. Unit- and
 * ability-ready events resolve an ability, while action-result events dispatch
 * the queued per-unit result. The meaning of 0x400 remains uncertain. */
static void battle_action_run_between_turn_events(void) {
    battle_unit_t* unit;
    s32 action;
    s32 runtime_id;

    action = battle_action_run_between_turn_control(BATTLE_BETWEEN_TURN_MODE_RUN);
    runtime_id = action & 0xff;
    g_battle_action_turn_event_type = action & 0xff00;
    if (g_battle_action_turn_event_type == BATTLE_TURN_EVENT_NONE) {
        return;
    }
    unit = battle_unit_get_by_battle_id(runtime_id);
    if (unit != 0) {
        g_battle_unit_source_runtime_id = unit->runtime_id;
    } else {
        main_system_handle_pointer_exception(12);
    }
    switch (g_battle_action_turn_event_type) {
    case BATTLE_TURN_EVENT_UNIT_READY:
    case BATTLE_TURN_EVENT_ABILITY_READY:
        battle_unit_draw_and_activate_by_runtime_id(unit->runtime_id);
        break;
    case BATTLE_TURN_EVENT_ACTION_RESULT:
        unit->pending_attack_result = battle_action_finalize_attack_and_flag_reactions(runtime_id);
        if (unit->pending_attack_result == -1) {
            battle_unit_find_relocation_tile(runtime_id, &unit->dismount);
            battle_unit_set_map_coords_after_death_dismount(unit);
        }
        break;
    case BATTLE_TURN_EVENT_UNKNOWN_0400:
        unit->pending_attack_result = battle_action_finalize_attack_and_flag_reactions(runtime_id);
        if (unit->pending_attack_result == -1) {
            battle_unit_find_relocation_tile(runtime_id, &unit->dismount);
            battle_unit_set_map_coords_after_death_dismount(unit);
        }
        battle_gfx_prepare_post_action_display_by_runtime_id(unit->runtime_id);
        battle_unit_update_display_by_runtime_id(unit->runtime_id);
        break;
    case BATTLE_TURN_EVENT_MIME:
        break;
    case 0x600: /* a label above 0x500 on the error path makes GCC split the upper
                   tree at 0x500 as the target does; nothing compares it, so the
                   original value is not recoverable */
    default:
        main_system_handle_pointer_exception(20);
        break;
    }
    unit = battle_unit_get_source();
    if (unit != 0) {
        unit->turn_action_flags = 0;
        if (g_battle_action_turn_event_type != BATTLE_TURN_EVENT_UNKNOWN_0400) {
            battle_target_move_cursor_to_unit(unit);
        }
    }
}

/* Runs any in-between-turn event, then opens the next acting unit's turn. */
void battle_action_check_between_turn_events(void) {
    battle_unit_t* unit;

    if (battle_map_try_start_deep_dungeon_mesh_load() != 0) {
        return;
    }
    battle_action_clear_at_list_id();
    g_main_system_animation_speed = 1;
    battle_action_run_between_turn_events();
    unit = battle_unit_get_source();
    if (unit == 0) {
        return;
    }
    g_battle_casting_runtime_id = unit->runtime_id;
    battle_unit_store_entd_flags_without_control_flag(unit);
    if (unit->stats != 0) {
        menu_reset_unit_record(unit->stats->battle_id);
        battle_unit_store_entd_flags_without_control_flag(unit);
        if (g_battle_menu_status_requested != 0 && !(unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)
            && g_battle_action_turn_event_type == BATTLE_TURN_EVENT_UNIT_READY) {
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            main_sound_play_sfx(MAIN_SFX_STATUS_MENU_REQUEST);
            battle_action_open_status_menu_at_list_start();
            battle_action_show_next_at_list_entry();
            battle_menu_set_next_script_action_menus();
            return;
        }
        g_battle_action_post_action = 0;
        if (g_battle_action_turn_event_type == BATTLE_TURN_EVENT_UNIT_READY
            || g_battle_action_turn_event_type == BATTLE_TURN_EVENT_NONE) {
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
        }
        if (battle_state_try_enter_scenario_event(unit) == 0) {
            menu_start_unit_view_thread(UNIT_VIEW_MODE_TILE_INFO, 0xff, 0xff);
            g_battle_game_state = BATTLE_GAME_STATE_CHANGE_TURN;
            battle_ai_init_selected_action();
            g_battle_menu_status_enabled = 1;
        } else {
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
        }
        g_battle_menu_status_requested = 0;
    } else {
        battle_state_set_free_cursor();
    }
}

void battle_state_enter_status_execute(void) {
    battle_unit_t* unit;
    u32 statuses;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_STATUS_EXECUTE;
    unit = battle_unit_get_source();
    battle_action_clear_at_list_active();
    g_battle_action_post_action = 0;
    if (g_battle_action_turn_event_type == BATTLE_TURN_EVENT_MIME) {
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
static void battle_state_restart_menu_to_targeting(void) {
    battle_unit_t* unit;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MENU_TO_TARGETING;
    unit = battle_unit_get_source();
    unit->state_frame_counter = 0;
    unit->command_ready = 0;
    battle_target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 0);
    if (unit != 0 && unit->stats != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, unit->stats->battle_id, 0);
    }
}

/* Enter the highlight-units state: tint every unit by its team, save the cursor
 * input mode and switch to the free cursor with map panning. */
void battle_state_enter_highlight_units_by_team(void) {
    battle_camera_enable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_HIGHLIGHT_UNITS;
    g_battle_cursor_input_mode_saved = g_battle_cursor_input_mode;
    g_battle_cursor_input_mode = BATTLE_CURSOR_INPUT_FREE_CURSOR;
    battle_gfx_tint_all_units_by_team();
    battle_action_clear_at_list_active();
}

/* Open the source unit's own action menu: the unit becomes the casting unit, the
 * cursor returns to it, the idling-action-menus state begins with the unit
 * summary panel, and the menu for its remaining move and action is built. */
void battle_menu_open_active_unit_idle_action_menu(void) {
    battle_unit_t* unit;

    battle_camera_disable_map_pan();
    unit = battle_unit_get_source();
    g_battle_casting_runtime_id = unit->runtime_id;
    battle_target_move_cursor_to_unit(unit);
    g_battle_game_state = BATTLE_GAME_STATE_IDLING_ACTION_MENUS;

    if (unit != 0 && unit->stats != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, unit->stats->battle_id, 0);
    }
    menu_build_idle_action_menu(battle_menu_get_id_based_on_mount_moveable_actable(unit->stats->battle_id));
}

/* Opens the non-active-unit idle action menu for the unit under the cursor;
 * g_battle_status_menu_open switches menu ids 0xf/0x10 to 0x11/0x12. */
void battle_menu_open_non_active_unit_idle_action_menu(void) {
    battle_unit_t* runtime;
    unit_stats_t* unit;
    s32 menu_id;

    runtime = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (runtime == 0) {
        return;
    }
    battle_camera_disable_map_pan();
    unit = runtime->stats;
    g_battle_game_state = BATTLE_GAME_STATE_IDLING_ACTION_MENUS;
    menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, unit->battle_id, 0);
    menu_id = battle_menu_get_id_based_on_mount_moveable_actable(runtime->stats->battle_id);
    if (g_battle_status_menu_open != 0) {
        if (menu_id == BATTLE_MENU_ID_PLAYER_UNIT) {
            menu_id = BATTLE_MENU_ID_PLAYER_UNIT_INSPECT;
        } else if (menu_id == BATTLE_MENU_ID_OTHER_UNIT) {
            menu_id = BATTLE_MENU_ID_OTHER_UNIT_INSPECT;
        }
    }
    menu_build_idle_action_menu(menu_id);
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
    if (g_battle_status_menu_open == 0 && unit->runtime_id == g_battle_unit_source_runtime_id) {
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
    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ACTION_HELP_MENU;
    g_battle_menu_help_opening = 1;
    menu_start_building_thread();
}

void battle_target_show_move_range_in_free_cursor(void) {
    battle_unit_t* unit;
    unit_stats_t* stats;
    s32 prev;

    battle_camera_enable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_DISPLAY_MOVE_AREA;
    unit = battle_unit_get_casting();
    stats = unit->stats;
    battle_move_set_reachable_tiles(stats->battle_id, unit->map_x, unit->map_y, unit->map_z);
    battle_target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_MOVE, 1);
    prev = g_battle_cursor_input_mode;
    g_battle_cursor_input_mode = BATTLE_CURSOR_INPUT_FREE_CURSOR;
    g_battle_cursor_input_mode_saved = prev;
    main_sound_play_sfx(MAIN_SFX_CONFIRM);
}

void battle_menu_open_free_cursor_mini_menu(void) {
    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MINI_MENU;
    menu_start_mini_menu_display_thread();
}

/* Leave the view of a unit picked from the AT list: enter the mini-menu state,
 * remove the tile marks and release OPTION.OUT's AT list thread, which waits on
 * g_menu_at_list_selection_finished. */
void battle_menu_open_mini_menu(void) {
    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MINI_MENU;
    battle_target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 3);
    menu_finish_at_list_selection();
}

void battle_menu_open_mini_menu_help(void) {
    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MINI_MENU_HELP;
    g_battle_menu_help_opening = 1;
    menu_start_building_thread();
}

void battle_action_get_next_acting_unit(s32 at_index) {
    s32 id;
    battle_unit_t* unit;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ACTIVE_TURN;
    id = battle_action_get_at_list_entry_unit_id(at_index);
    if (id >= 0) {
        unit = battle_unit_get_by_battle_id(id & 0xFF);
        if ((id & 0x100) != 0) {
            battle_target_move_cursor_to_action_target(unit);
            battle_target_calculate_for_menu_types(&unit->stats->action_command);
            battle_target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_TARGETS, 3);
        } else {
            battle_target_move_cursor_to_unit(unit);
            battle_target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 0);
        }
    }
    battle_target_store_cursor_unit_name_and_data();
    battle_action_set_at_list_active();
}

/* Enter the unit-move state and show the source unit's targeting prompt. */
void battle_action_handle_move_command(void) {
    battle_unit_t* unit;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_UNIT_MOVE;
    unit = battle_unit_get_source();
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 0, unit->stats->battle_id, 0,
        unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Enter the move-range-exception state, reached when the casting unit has no
 * reachable tile: opens targeting-prompt option 2 for the source unit. */
static void battle_state_enter_move_range_exception(void) {
    battle_unit_t* unit;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MOVE_RANGE_EXCEPTION;
    unit = battle_unit_get_source();
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 2, unit->stats->battle_id, 0,
        unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Return to choosing a move destination: recompute the source unit's reachable
 * tiles, show them and enter the close-move-help state with the cursor unit's
 * panel. With no reachable tile the move-range exception state is entered
 * instead. The AT-list-active flag follows whether the player controls the unit. */
void battle_menu_close_move_help(void) {
    battle_unit_t* unit;
    battle_unit_t* casting;

    battle_camera_disable_map_pan();
    unit = battle_unit_get_source();
    casting = battle_unit_get_casting();
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        battle_action_set_at_list_active();
    } else {
        battle_action_clear_at_list_active();
    }

    if (battle_move_set_reachable_tiles(casting->stats->battle_id, casting->map_x, casting->map_y, casting->map_z)
        > 0) {
        battle_target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_MOVE, 1);
        g_battle_game_state = BATTLE_GAME_STATE_CLOSE_MOVE_HELP;
        casting->state_frame_counter = 0;
        battle_target_store_cursor_unit_name_and_data();
    } else {
        battle_state_enter_move_range_exception();
    }
}

/* Enter the illegal-move state and show move feedback option 2, which rejects the
 * chosen destination. */
void battle_menu_open_illegal_move_help(void) {
    battle_unit_t* unit;
    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ILLEGAL_MOVE_MENU;
    unit = battle_unit_get_source();
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, 2, unit->stats->battle_id, 0,
        unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
}

void battle_state_enter_unit_moving_setup(void) {
    battle_unit_t* src;
    battle_unit_t* cast;
    battle_unit_t* at_tile;
    u8 path_count;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_UNIT_MOVING_SETUP;
    src = battle_unit_get_source();
    cast = battle_unit_get_casting();

    if (cast->path.pending_mount & BATTLE_PENDING_MOUNT_SET) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, 1, src->stats->battle_id, 0,
            src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        return;
    }
    path_count = cast->path.count + 2;
    if (path_count < 2) {
        if (g_main_game_options.fields.navigation_messages != GAME_OPTION_ON) {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, 0, src->stats->battle_id, 0,
                src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
            return;
        }
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM, 0, src->stats->battle_id, 0,
            src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        return;
    }
    at_tile
        = battle_unit_get_crystal_or_treasure_at_map_coords(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (at_tile != 0) {
        if (at_tile->status_flags_5_6 & BATTLE_UNIT_STATUS_CRYSTAL) {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, 4, src->stats->battle_id, 0,
                src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
            return;
        }
    }
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, 0, src->stats->battle_id, 0,
        src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Saves the unit's tile, facing, mount link and stood-on unit before it moves;
 * move_return_to_previous_map_square restores them. */
static void battle_unit_save_previous_state(battle_unit_t* unit) {
    u8 x = unit->map_x;
    u8 y = unit->map_y;
    u8 z = unit->map_z;
    u16 facing = unit->facing;
    u8 mount_value = unit->mount_state;
    u8 mount_id = unit->mount_partner_runtime_id;
    u8 standing_on_battle_id_plus_one = unit->standing_on_battle_id_plus_one;

    unit->previous_map_x = x;
    unit->previous_map_y = y;
    unit->previous_map_z = z;
    unit->previous_facing = facing;
    unit->previous_mount_state = mount_value;
    unit->previous_mount_partner_runtime_id = mount_id;
    unit->previous_standing_on_battle_id_plus_one = standing_on_battle_id_plus_one;
}

/* Enter the unit-moving state: save the casting unit's tile, facing and mount
 * state for an undo (and its partner's when it is a mount) and start its path
 * from the first step at speed 0x2000. */
void battle_state_enter_unit_moving(void) {
    battle_unit_t* unit;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_UNIT_MOVING;
    battle_target_store_cursor_unit_name_and_data();
    unit = battle_unit_get_casting();
    battle_unit_save_previous_state(unit);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
        battle_unit_t* mount = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (mount != 0) {
            battle_unit_save_previous_state(mount);
        }
    }
    unit->step_speed = 0x2000;
    unit->movement_path_offset = 0;
    g_battle_state_animation_continue_check = 0;
    battle_action_clear_at_list_active();
}

/* Enter the move-confirm state once the unit has stopped moving and show the
 * matching prompt, with a one-second message time.
 *
 * A failed Teleport shows effect message 0x183b; a successful one the move
 * confirmation. Other movement tints the unit by team and asks for confirmation,
 * silently when navigation messages are off. The AT-list-active flag follows
 * whether the player controls the unit. */
void battle_menu_open_move_confirm(void) {
    battle_unit_t* source;
    battle_unit_t* caster;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MOVE_CONFIRM_MENU;
    source = battle_unit_get_source();
    caster = battle_unit_get_casting();
    text_set_message_duration_frames(0x3C);
    if (caster->path.flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT) {
        if (caster->last_path_count == MOVE_PATH_TELEPORT_FAILED) {
            menu_init_system_function(
                BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x183B, source->stats->battle_id, 0, 1);
        } else {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM, 0, source->stats->battle_id, 0, 0);
        }
        caster->last_path_count = 0;
    } else {
        if (g_main_game_options.fields.navigation_messages != GAME_OPTION_ON) {
            battle_gfx_modify_palette_based_on_team(source, 2);
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM_SILENT, 0, source->stats->battle_id, 0,
                source->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            battle_gfx_modify_palette_based_on_team(source, 2);
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM, 0, source->stats->battle_id, 0,
                source->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        }
    }
    if (source->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
        battle_action_set_at_list_active();
    } else {
        battle_action_clear_at_list_active();
    }
}

void battle_state_enter_after_command(void) {
    g_main_system_animation_speed = 1;
    g_battle_game_state = BATTLE_GAME_STATE_AFTER_COMMAND;
    g_battle_action_post_action = 0;
    battle_target_move_cursor_to_unit(battle_unit_get_source());
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_CLOSE, 0, 0, 0, 0);
}

/* Turn a unit to one of the four cardinal facings and restart its animation.
 * A diagonal or any other angle leaves it as it is. */
void battle_unit_decide_facing_direction(battle_unit_t* unit, s32 facing) {
    switch (facing) {
    case BATTLE_FACING_NORTH:
        if (unit->facing == BATTLE_FACING_NORTH) {
            return;
        }
        unit->facing = BATTLE_FACING_NORTH;
        break;
    case BATTLE_FACING_SOUTH:
        if (unit->facing == BATTLE_FACING_SOUTH) {
            return;
        }
        unit->facing = BATTLE_FACING_SOUTH;
        break;
    case BATTLE_FACING_WEST:
        if (unit->facing == BATTLE_FACING_WEST) {
            return;
        }
        unit->facing = BATTLE_FACING_WEST;
        break;
    case BATTLE_FACING_EAST:
        if (unit->facing == BATTLE_FACING_EAST) {
            return;
        }
        unit->facing = BATTLE_FACING_EAST;
        break;
    case BATTLE_FACING_SOUTH_WEST:
    case BATTLE_FACING_NORTH_WEST:
    case BATTLE_FACING_NORTH_EAST:
    case BATTLE_FACING_SOUTH_EAST:
    default:
        return;
    }
    battle_unit_set_animation_based_on_status(unit);
}

/* Report a level up recorded in the unit's rewards: message 0x183f for three
 * seconds, the level-up animation and effect, the cursor on the unit and the
 * level-up tune. Returns 1 after reporting one, otherwise 0. */
static s32 battle_action_report_level_up(battle_unit_t* unit) {
    if (unit->strike.action_rewards.level_for_display != 0) {
        u8 battle_id;
        text_set_message_duration_frames(0xB4);
        battle_id = unit->stats->battle_id;
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x183F, battle_id, battle_id, 1);
        battle_unit_set_level_up_animation(unit);
        battle_target_move_cursor_to_unit(unit);
        battle_effect_set_secondary_level_up(unit);
        main_sound_play_tune(MAIN_SOUND_TUNE_LEVEL_UP);
        return 1;
    }
    return 0;
}

/* Report a job level up recorded in the unit's rewards: message 0x183e for three
 * seconds, the level-up animation and effect, the cursor on the unit and the job
 * level-up tune. Returns 1 after reporting one, otherwise 0. */
static s32 battle_action_report_job_level_up(battle_unit_t* unit) {
    if (unit->strike.action_rewards.job_level_for_display != 0) {
        u8 battle_id;
        text_set_message_duration_frames(0xB4);
        battle_id = unit->stats->battle_id;
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x183E, battle_id, battle_id, 1);
        battle_unit_set_level_up_animation(unit);
        battle_target_move_cursor_to_unit(unit);
        battle_effect_set_secondary_level_up(unit);
        main_sound_play_tune(MAIN_SOUND_TUNE_JOB_LEVEL_UP);
        return 1;
    }
    return 0;
}

/* Starts the next post-action step selected by g_battle_move_find_result.
 *
 * Handles, in priority order, the crystal/treasure pickup (bit 0x1), the
 * EXP/JP and level-up reports (0x2), a found rare/common item (0x4), a tile
 * trap (0x8), a canceled charge (0x10) and the post-action displays for the
 * source (0x200) or its mount partner (0x400). Returns 1 when a step was
 * started, otherwise refreshes both units, re-enters the after-command state
 * unless the Deep Dungeon map refresh takes over, and returns 0. */
s32 battle_move_start_next_post_movement_step(void) {
    battle_unit_t* source;
    battle_unit_t* casting;
    battle_unit_t* target;
    unit_stats_t* stats;
    s32 item;
    s32 animation;
    s32 trap;

    source = battle_unit_get_source();
    casting = battle_unit_get_casting();
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
    g_battle_game_state = BATTLE_GAME_STATE_CRYSTAL_LEARN;
    g_battle_action_post_action = 0;
    casting->state_frame_counter = 0;
    if (g_battle_move_find_result & MOVE_POST_EVENT_CRYSTAL_OR_TREASURE) {
        target = battle_unit_get_crystal_or_treasure_at_map_coords(source->map_x, source->map_y, source->map_z);
        item = menu_get_dead_unit_selection(source->stats->battle_id);
        if (item == -1) {
            animation = BATTLE_UNIT_ANIMATION_RAISE_HANDS;
        } else {
            battle_gfx_store_item_display_data(source, item);
            animation = BATTLE_UNIT_ANIMATION_HOLD_ITEM;
        }
        battle_unit_store_animation_facing_movement_data(animation, source->facing, source);
        if (item == -1) {
            stats = source->stats;
            if (!(stats->initial_team_flags & BATTLE_TEAM_MASK)
                && (stats->initial_team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)) {
                menu_init_system_function(
                    BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, stats->battle_id, stats->battle_id, 0, 1);
            } else {
                menu_init_system_function(
                    BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, stats->battle_id, stats->battle_id, 0, 0);
                battle_unit_learn_from_crystal(source->stats, BATTLE_CRYSTAL_RESTORE_HP_MP);
                battle_action_finalize_attack_and_flag_reactions(source->stats->battle_id);
                battle_gfx_prepare_post_action_display(source);
                battle_unit_update_display_by_runtime_id(source->runtime_id);
            }
        } else {
            if (!(source->stats->initial_team_flags & BATTLE_TEAM_MASK)) {
                menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, source->stats->battle_id,
                    source->stats->battle_id, 0, 1);
            } else {
                menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, source->stats->battle_id,
                    source->stats->battle_id, 0, 0);
            }
            if ((source->stats->initial_team_flags & BATTLE_TEAM_MASK)
                || !(source->stats->initial_team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)) {
                battle_unit_learn_from_crystal(source->stats, BATTLE_CRYSTAL_TAKE_TREASURE);
            }
        }
        battle_gfx_start_unit_fade_out_red(target->runtime_id);
        target->depth_height_offset
            = 0x24 - (battle_gfx_calculate_world_height_from_world_coords(source) - target->world_coords.vy);
        main_sound_play_sfx_find_channel(MAIN_SFX_COMPLETE);
        return 1;
    }
    if (g_battle_move_find_result & MOVE_POST_EVENT_MOVEMENT_BENEFIT) {
        switch (g_battle_action_post_action_display_phase) {
        case BATTLE_POST_ACTION_DISPLAY_LEVEL_UP:
            battle_action_init_movement_ability_benefit(casting->stats);
            battle_unit_build_gained_exp_jp_level_job_level(casting->stats, &casting->strike.action_rewards);
            battle_gfx_prepare_post_action_display(casting);
            battle_gfx_init_earned_exp_jp_display(casting);
            battle_action_report_level_up(casting);
            return 1;
        case BATTLE_POST_ACTION_DISPLAY_JOB_LEVEL_UP:
            battle_action_report_job_level_up(casting);
            return 1;
        }
        return 1;
    }
    if (g_battle_move_find_result & MOVE_POST_EVENT_ITEM_FOUND) {
        g_battle_move_find_display_item = *battle_map_determine_rare_common_item(casting->stats);
        battle_gfx_store_item_display_data(source, g_battle_move_find_display_item.item_id);
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_FOUND_ITEM, source->facing, source);
        if (!(source->team_flags & BATTLE_TEAM_MASK)) {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FIND_ITEM,
                g_battle_move_find_display_item.item_id, source->stats->battle_id, 0, 1);
            main_sound_play_tune(MAIN_SOUND_TUNE_ITEM_FOUND);
            return 1;
        }
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FIND_ITEM, g_battle_move_find_display_item.item_id,
            source->stats->battle_id, 0, 0);
        return 1;
    }
    if (g_battle_move_find_result & MOVE_POST_EVENT_TRAP_TRIGGERED) {
        trap = battle_action_apply_tile_trap(casting->stats);
        if ((source->pending_attack_result = battle_action_finalize_attack_and_flag_reactions(source->stats->battle_id))
            == -1) {
            battle_unit_find_relocation_tile(source->stats->battle_id, &source->dismount);
            battle_unit_set_map_coords_after_death_dismount(source);
        }
        battle_gfx_prepare_post_action_display(source);
        battle_unit_set_target_animation_from_attack_type(0, source);
        battle_effect_set_secondary_venom_trap(source);
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TRAP_RESULT, trap, source->stats->battle_id, 0, 1);
        battle_ai_record_triggered_trap_coords(casting->map_x, casting->map_z, casting->map_y);
        return 1;
    }
    if (g_battle_move_find_result & MOVE_POST_EVENT_CHARGING_CANCEL) {
        battle_status_remove_charging_ability_ct(casting->stats, 1);
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
        return 1;
    }
    if (g_battle_move_find_result & MOVE_POST_EVENT_SOURCE_DISPLAY) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
        battle_gfx_prepare_post_action_display_by_runtime_id(source->runtime_id);
        battle_unit_update_display_by_runtime_id(source->runtime_id);
        return 1;
    }
    if (g_battle_move_find_result & MOVE_POST_EVENT_MOUNT_STATUS_CHANGE) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
        if (source->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
            target = battle_unit_get_by_runtime_id(source->mount_partner_runtime_id);
            if (target != 0) {
                battle_gfx_prepare_post_action_display_by_runtime_id(target->runtime_id);
                battle_unit_update_display_by_runtime_id(target->runtime_id);
            }
        }
        return 1;
    }
    battle_unit_update_display_by_runtime_id(source->runtime_id);
    battle_unit_update_display_by_runtime_id(casting->runtime_id);
    if (battle_map_refresh_deep_dungeon() == 0) {
        battle_state_enter_after_command();
    }
    return 0;
}

/* Stop the game flow after the casting unit's move and start the post-movement
 * steps.
 *
 * move_set_target_for_mounted_unit_and_find_item builds the step flags in
 * g_battle_move_find_result (0 for a unit without battle data), and the
 * post-action display starts at the level-up report. When any step is pending the
 * map cursor and its tile glow are hidden. */
void battle_move_init_post_movement_display(void) {
    unit_stats_t* stats;

    battle_camera_disable_map_pan();
    stats = battle_unit_get_casting()->stats;
    if (stats != 0) {
        g_battle_move_find_result = battle_move_set_target_for_mounted_unit_and_find_item(stats);
    } else {
        g_battle_move_find_result = 0;
    }
    g_battle_action_post_action_display_phase = BATTLE_POST_ACTION_DISPLAY_LEVEL_UP;
    battle_move_start_next_post_movement_step();
    if (g_battle_move_find_result != 0) {
        battle_action_clear_at_list_active();
    }
    g_battle_state_animation_continue_check = 0;
}

void battle_action_choose_facing_for_wait(void) {
    battle_unit_t* mounted_unit;
    battle_unit_t* unit;
    s32 facing;

    battle_camera_disable_map_pan();
    unit = battle_unit_get_source();
    facing = unit->facing;
    g_battle_game_state = BATTLE_GAME_STATE_WAIT_DIRECTION;
    g_battle_state_wait_direction_facing = facing;
    unit->state_frame_counter = 0;
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        mounted_unit = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (mounted_unit != 0) {
            battle_unit_decide_facing_direction(unit, mounted_unit->facing);
            if (unit->stats != 0) {
                battle_unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z,
                    (mounted_unit->facing / BATTLE_FACING_QUARTER_TURN));
            }
        }
    }
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) == 0) {
        unit->command_state.cursor.facing_hint = battle_ai_choose_wait_facing();
    }
}

/* Open the Wait menu for the source unit, or skip it.
 *
 * A scenario check that finds a pending event whose finish operation is nonzero
 * starts that event instead. A unit whose turn status blocks the Wait menu goes
 * straight to the between-turn events. Otherwise the wait-menu state opens with
 * the cursor unit's panel. The AT-list-active flag follows whether the player
 * controls the unit. */
void battle_action_choose_wait(void) {
    battle_unit_t* unit;

    battle_camera_disable_map_pan();
    unit = battle_unit_get_source();
    if (menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 2, unit->stats->battle_id, 0, 1) == 2
        && script_get_event_finish_operation() != 0) {
        g_battle_previous_game_state = g_battle_game_state;
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, unit->stats->battle_id, 0, 1);
        battle_action_set_casting_unit_id_ff();
        return;
    }
    if (battle_status_calculate_turn_flags(unit->stats) & BATTLE_TURN_STATUS_BLOCKS_WAIT_MENU_MASK) {
        battle_action_check_between_turn_events();
    } else {
        g_battle_game_state = BATTLE_GAME_STATE_WAIT_MENU;
        battle_target_store_cursor_unit_name_and_data();
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_WAIT_MENU, 0, unit->stats->battle_id, 0,
            unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    }
    if (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
        battle_action_set_at_list_active();
    } else {
        battle_action_clear_at_list_active();
    }
}

/* Face a renderer unit toward the current cursor tile.
 *
 * The 14-pixel adjustment compares the projected unit position with the tile
 * center; masking the fixed-point angle selects one of four cardinal facings. */
void battle_unit_face_toward_cursor(battle_unit_t* unit) {
    VECTOR difference;
    s32 unit_x;
    s32 unit_y;

    if (unit != 0) {
        s32 cursor_x = g_battle_cursor_x;
        s32 cursor_y = g_battle_cursor_y;
        s32 tile_x;
        s32 tile_y;
        difference.vz = 0;
        difference.vx = 0;
        tile_x = cursor_x * MAP_TILE_SIZE;
        unit_x = unit->world_coords.vx - MAP_TILE_SIZE / 2;
        difference.vx = tile_x - unit_x;
        tile_y = cursor_y * MAP_TILE_SIZE;
        unit_y = unit->world_coords.vz - MAP_TILE_SIZE / 2;
        difference.vz = tile_y - unit_y;
        if (difference.vx != 0 || difference.vz != 0) {
            unit->facing = (ratan2(-difference.vz, difference.vx) - 0x200) & 0xC00;
        }
    }
}

/* If the unit has no attack facing yet (0xffff), seed it from the unit's
 * current facing so the attack animation turns towards the same direction.
 * The target reads attack_facing with a signed `lh`, hence the s16 access. */
static void battle_unit_save_facing_before_attack(battle_unit_t* unit) {
    s16 facing;
    char unused[16];

    if (unit != 0) {
        if (unit->attack_facing == -1) {
            facing = unit->facing;
            unit->attack_facing = facing;
        }
    }
}

/* Turn a unit to face a target unit, or its action's target.
 *
 * Without an explicit target, targeting type 5 aims at the center of the
 * action's target tile (28 screen units per tile) and other actions at the
 * target unit. The facing snaps to a quadrant, is copied to the mount/rider
 * partner, and is written back to the unit's battle position; units with
 * Sleep, Petrify, Stop, Dead, Don't Act, or reaction_ability_id 0x1b8 (Reflect) keep their
 * facing. */
void battle_unit_face_towards_action_target(battle_unit_t* unit, battle_unit_t* target) {
    VECTOR delta;
    battle_unit_t* other;

    if (unit == 0) {
        return;
    }
    delta.vz = 0;
    delta.vx = 0;
    if (target != 0) {
        delta.vx = target->world_coords.vx - unit->world_coords.vx;
        delta.vz = target->world_coords.vz - unit->world_coords.vz;
    } else if (unit->stats->action_command.targeting_type == BATTLE_TARGETING_TYPE_TILE) {
        delta.vx = unit->stats->action_command.target_position.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2)
            - unit->world_coords.vx;
        delta.vz = unit->stats->action_command.target_position.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2)
            - unit->world_coords.vz;
    } else {
        other = battle_unit_get_by_battle_id(unit->stats->action_command.target_id);
        if (other != 0) {
            delta.vx = other->world_coords.vx - unit->world_coords.vx;
            delta.vz = other->world_coords.vz - unit->world_coords.vz;
        }
    }
    if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_PREVENT_FACING_MASK) {
        return;
    }
    if (unit->strike.reaction_ability_id == ABILITY_ID_REACTION_REFLECT) {
        return;
    }
    if (delta.vx == 0 && delta.vz == 0) {
        return;
    }
    battle_unit_save_facing_before_attack(unit);
    unit->facing = (ratan2(-delta.vz, delta.vx) - 0x200) & 0xc00;
    battle_unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, unit->facing / 1024);
    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
        battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id)->facing = unit->facing;
        battle_unit_set_tile_position(
            unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, unit->facing / 1024);
    }
}

/* Turns a living unit back to the facing saved in attack_facing (-1 for none) and
 * clears the saved value; a dead unit only has it cleared.
 *
 * The battle stats take the facing in quarter turns through
 * battle_unit_set_tile_position. For a mounted pair the partner record's facing
 * is copied too, without a null test, and the tile position is stored a second
 * time under the unit's own id. */
static void battle_unit_restore_facing_after_attack(battle_unit_t* unit) {
    battle_unit_t* mount;
    s16 rot;
    s32 unused[4];

    if (unit != 0) {
        if ((unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEAD) == 0) {
            /* The facing 16-bit values are read signed (lh) here. */
            rot = unit->attack_facing;
            if (rot != -1) {
                unit->facing = rot;
                battle_unit_set_tile_position(
                    unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, rot / 1024);
                /* Stored as a signed 16-bit value (li -1, not ori 0xffff). */
                unit->attack_facing = -1;
                if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
                    mount = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                    mount->facing = unit->facing;
                    battle_unit_set_tile_position(
                        unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, unit->facing / 1024);
                }
            }
        } else {
            unit->attack_facing = -1;
        }
    }
}

/* Restore the facing each unit in the attacker's target list had before the
   attack. */
void battle_unit_restore_target_facings_after_attack(battle_unit_t* unit) {
    s32 i;
    battle_unit_t* target;

    if (unit->strike.target_count != 0) {
        i = 0;
        while (i < unit->strike.target_count) {
            target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
            if (target != 0) {
                battle_unit_restore_facing_after_attack(target);
            }
            i++;
        }
    }
}

/* battle_unit_restore_facing_after_attack for every runtime id 0..15. */
static void battle_unit_restore_all_facings_after_attack(void) {
    s32 i;

    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        battle_unit_t* unit = battle_unit_get_by_runtime_id(i & 0xFFFF);
        if (unit) {
            battle_unit_restore_facing_after_attack(unit);
        }
    }
}

/* Enter the JP/EXP gain state at animation speed 1 and restart the source unit's
 * state frame counter. */
void battle_state_enter_jp_xp_gain(void) {
    battle_unit_t* unit;

    g_main_system_animation_speed = 1;
    g_battle_game_state = BATTLE_GAME_STATE_JP_EXP_GAIN;
    unit = battle_unit_get_source();
    if (unit != 0) {
        unit->state_frame_counter = 0;
    }
}

/* Ends a unit's command: the cursor returns to the unit, its display is updated
 * and the game enters the after-command state. */
void battle_unit_update_post_command_animation_display(battle_unit_t* unit) {
    battle_target_move_cursor_to_unit(unit);
    battle_unit_update_display_by_runtime_id(unit->runtime_id);
    battle_state_enter_after_command();
}

/*
 * Enter action-execution setup after panel preparation.
 *
 * Panel result 3 selects the alternate setup argument. AT-list state
 * follows the unit's control flag after the setup call returns.
 */
void battle_state_enter_action_execution_setup(void) {
    battle_unit_t* unit;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ACTION_EXECUTE_SETUP;
    unit = battle_unit_get_source();
    if (unit->ability_preview_phase == TARGET_PANELS_NONE)
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 1, unit->stats->battle_id, 0,
            unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    else
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 0, unit->stats->battle_id, 0,
            unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    if (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)
        battle_action_set_at_list_active();
    else
        battle_action_clear_at_list_active();
}

/* Enter the action-execute state for the casting unit, point the cursor at its
 * action's target and release the ability's effect. The secondary-effect state
 * calls it every frame.
 *
 * A plain attack (ability 0), the knockback strike (ABILITY_ID_KNOCKBACK) and an
 * ability without an effect file play nothing. Accumulate and the Charge
 * abilities play the effect at animation speed 1 unless it is an item effect.
 * Every other ability plays at animation speed 2 with the tracked sound effects
 * paused; an item effect is held back until the unit shows its hold-item
 * animation. The AT-list-active flag is cleared on every path. */
void battle_action_set_item_throw_stone_ability_display(void) {
    battle_unit_t* unit;
    s32 ability;

    g_battle_game_state = BATTLE_GAME_STATE_ACTION_EXECUTE;
    unit = battle_unit_get_casting();
    ability = unit->used_ability_id;
    unit->state_frame_counter = 0;
    battle_target_move_cursor_to_action_target(unit);

    if ((ability == 0) || (ability == ABILITY_ID_KNOCKBACK)) {
        g_main_system_animation_speed = 1;
        battle_action_clear_at_list_active();
        return;
    }
    if (battle_effect_load_ability(ability) != 0) {
        g_main_system_animation_speed = 1;
        battle_action_clear_at_list_active();
        return;
    }
    if ((ability == ABILITY_ID_BASIC_SKILL_ACCUMULATE)
        || (ability >= ABILITY_ID_CHARGE_FIRST && ability < ABILITY_ID_MATH_FIRST)) {
        if (battle_effect_is_item_ability(ability) != 0) {
            g_main_system_animation_speed = 1;
            battle_action_clear_at_list_active();
            return;
        }
        battle_effect_play();
        g_main_system_animation_speed = 1;
        battle_action_clear_at_list_active();
        return;
    }
    if ((battle_effect_is_item_ability(ability) == 0)
        || ((unit->animation.sequence_id >> 1) == BATTLE_UNIT_ANIMATION_HOLD_ITEM)) {
        battle_effect_play();
    }
    main_sound_stop_current_weather_sfx();
    g_main_system_animation_speed = 2;
    battle_action_clear_at_list_active();
}

/* Enter the resume-attack-phase state and show the last queued post-effect
 * message, if any, as a post-action result. Returns 1 after showing a message and
 * 0 when the queue is empty. */
s32 battle_action_resume_attack_phase_control(void) {
    s32 idx;

    g_battle_game_state = BATTLE_GAME_STATE_RESUME_ATTACK_PHASE;
    g_main_system_animation_speed = 1;
    battle_unit_get_casting();
    idx = g_battle_action_post_effect_msg_counter;
    if (idx != 0) {
        idx -= 1;
        g_battle_action_post_effect_msg_counter = idx;
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_POST_ACTION_RESULT,
            g_battle_action_post_effect_msgs[idx].code, g_battle_action_post_effect_msgs[idx].unit,
            g_battle_action_post_effect_msgs[idx].value, 1);
        g_battle_action_post_action = 0;
        return 1;
    }
    return 0;
}

/*
 * Queue the post-action "effect message" popups for one unit.
 *
 * Walks the 11 reportable outcomes recorded in the unit's
 * action_data_t and appends one 8-byte record per outcome to the
 * queue at g_battle_action_post_effect_msgs: message code at +0, the acting unit's runtime ID at +1,
 * and the reported value at +4. g_battle_action_post_effect_msg_counter is
 * the record count, drained by action_resume_attack_phase_control.
 *
 * The 11-way dispatch really is a jump table in the target (at 0x80067548),
 * which is why the .ld places a .rodata output section there.
 */
static void battle_action_queue_post_effect_messages_for_unit(battle_unit_t* runtime) {
    action_data_t* action;
    s32 i;
    s32 queued;
    s32 off;

    if (runtime == 0) {
        return;
    }
    if (runtime->stats == 0) {
        return;
    }
    if (g_battle_action_post_effect_msg_counter >= 0x10) {
        return;
    }
    action = &runtime->stats->action;
    if (action->hit == 0) {
        return;
    }

    for (i = 0; i < 11; i++) {
        queued = 0;
        switch (i) {
        case 0:
            if (action->sp_change != 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x28;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->sp_change;
                queued = 1;
            }
            break;
        case 1:
            if (action->ct_change != 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x27;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->ct_change;
                queued = 1;
            }
            break;
        case 2:
            if (action->brave_change != 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x29;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->brave_change;
                queued = 1;
            }
            break;
        case 3:
            if (action->faith_change != 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x2a;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->faith_change;
                queued = 1;
            }
            break;
        case 4:
            if (action->pa_change != 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x2b;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->pa_change;
                queued = 1;
            }
            break;
        case 5:
            if (action->ma_change != 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x2c;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->ma_change;
                queued = 1;
            }
            break;
        case 6:
            if (action->gil_change > 0) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x2e;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->gil_change;
                queued = 1;
            }
            break;
        case 7:
            if (action->exp_change & 0x80) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x2f;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->exp_change;
                queued = 1;
            }
            break;
        case 8:
            if (action->special_effect & (ACTION_SPECIAL_EFFECT_LEVEL_UP | ACTION_SPECIAL_EFFECT_LEVEL_DOWN)) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x2d;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value
                    = action->special_effect;
                queued = 1;
            }
            break;
        case 9:
            if (action->special_effect
                & (ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN)) {
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x1d;
                off = g_battle_action_post_effect_msg_counter;
                g_battle_action_post_effect_msgs[off].value = action->item_lost;
                if (action->special_effect & ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN) {
                    g_battle_action_post_effect_msgs[off].value = action->item_lost | 0x8000;
                }
                queued = 1;
            }
            break;
        case 10:
            if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                queued = 1;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].code = 0x1c;
                g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].value = action->item_lost;
            }
            break;
        }
        if (queued != 0) {
            g_battle_action_post_effect_msgs[g_battle_action_post_effect_msg_counter].unit = runtime->stats->battle_id;
            g_battle_action_post_effect_msg_counter += 1;
        }
    }
}

/* Empty the post-effect message queue, then queue the messages of every target of
 * the casting unit. */
static void battle_action_build_targets_post_action_message(void) {
    battle_unit_t* base;
    s32 i;

    g_battle_action_post_effect_msg_counter = 0;
    base = battle_unit_get_casting();
    for (i = 0; i < base->strike.target_count; i++) {
        battle_action_queue_post_effect_messages_for_unit(battle_unit_get_by_battle_id(base->strike.target_list[i]));
    }
}

/* Queue the casting unit's own post-effect messages and show the first one. With
 * effect messages turned off, or nothing to report, an effect message with text
 * id 0 is requested instead. */
void battle_action_show_caster_post_effect_messages(void) {
    battle_unit_t* unit;

    unit = battle_unit_get_casting();
    battle_action_queue_post_effect_messages_for_unit(unit);
    if (g_main_game_options.fields.effect_messages != GAME_OPTION_ON) {
        g_battle_action_post_effect_msg_counter = 0;
    }
    if (battle_action_resume_attack_phase_control() == 0) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
}

/* Applies the prepared action result to a unit and shows it.
 *
 * The result code is kept in pending_attack_result; -1 (a rider killed on its
 * mount) finds a relocation tile and moves the unit there. The post-action
 * display and the display update follow. */
static void battle_unit_update_attack_result_animation(battle_unit_t* unit) {
    unit->pending_attack_result = battle_action_store_target_stats_pointer_data(unit->stats->battle_id);
    if (unit->pending_attack_result == -1) {
        battle_unit_find_relocation_tile(unit->stats->battle_id, &unit->dismount);
        battle_unit_set_map_coords_after_death_dismount(unit);
    }
    battle_gfx_prepare_post_action_display_by_runtime_id(unit->runtime_id);
    battle_unit_update_display_by_runtime_id(unit->runtime_id);
}

/* Enter BATTLE_GAME_STATE_BATTLE_MESSAGE_DISPLAY.
 *
 * Confirms the ability's targets, then opens the system message window for the
 * first target whose action actually connected.  The message id comes from the
 * ability animation table's third byte; 0x06 (and only 0x06) also plays tune 2,
 * and 0x06/0x2b are the two messages that name both units. */
void battle_state_start_battle_message_display(void) {
    battle_unit_t* unit;
    battle_unit_t* target;
    battle_ability_animation_data_t* animation;
    s32 message;
    s32 i;

    g_battle_game_state = BATTLE_GAME_STATE_BATTLE_MESSAGE_DISPLAY;
    g_main_system_animation_speed = 1;
    g_battle_action_post_action = 0;
    unit = battle_unit_get_casting();
    battle_action_build_targets_post_action_message();
    battle_unit_update_attack_result_animation(unit);
    unit->item_display_active = 0;
    text_set_message_duration_frames(0x3c);
    switch (unit->strike.post_action_message) {
    case BATTLE_POST_ACTION_MESSAGE_POACHED:
        main_sound_play_tune(MAIN_SOUND_TUNE_ITEM_FOUND);
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x1838, unit->stats->battle_id, 0, 1);
        return;
    case BATTLE_POST_ACTION_MESSAGE_TRAINED:
        main_sound_play_tune(MAIN_SOUND_TUNE_ITEM_FOUND);
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x1828, unit->stats->battle_id, 0, 1);
        return;
    }

    message = 0;
    for (i = 0; i < unit->strike.target_count; i++) {
        target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
        if (target != 0 && target->stats != 0 && target->stats->action.hit != 0) {
            text_set_message_duration_frames(0x3c);
            /* Taking the row first keeps this loop above loop.c's size limit
             * for hoisting the constant 6 into a saved register. */
            animation = &g_battle_ability_animation_data[unit->sp2_ability_id];
            message = animation->text_display;
            if (message == 0) {
                break;
            }
            if (message == 6) {
                main_sound_play_tune(MAIN_SOUND_TUNE_ITEM_FOUND);
            }
            break;
        }
    }

    if (message == 6 || message == 0x2b) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, message | 0x1800, unit->stats->battle_id,
            target->stats->battle_id, 1);
    } else if (message != 0) {
        menu_init_system_function(
            BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, message | 0x1800, unit->stats->battle_id, 0, 1);
    } else {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
}

/* Enter the post-action report state for the source unit's current phase.
 *
 * Phase 0 reports a level up, phase 1 a job level up, and phase 2 looks for a
 * unit that learns the used ability on hit; without one the phase advances. */
void battle_action_init_learn_ability_on_hit(void) {
    battle_unit_t* unit;
    battle_unit_t* learner;
    u16 ability_id;

    g_battle_game_state = BATTLE_GAME_STATE_LEARN_ABILITY_ON_HIT;
    g_main_system_animation_speed = 1;
    unit = battle_unit_get_source();
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    text_set_message_duration_frames(0x1E);
    switch (g_battle_action_post_action_display_phase) {
    case BATTLE_POST_ACTION_DISPLAY_LEVEL_UP:
        battle_action_report_level_up(unit);
        break;
    case BATTLE_POST_ACTION_DISPLAY_JOB_LEVEL_UP:
        battle_action_report_job_level_up(unit);
        break;
    case BATTLE_POST_ACTION_DISPLAY_LEARN_ON_HIT:
        g_battle_action_learn_on_hit_unit_id
            = battle_ability_find_learn_on_hit_unit(unit->stats->battle_id, &ability_id);
        if (g_battle_action_learn_on_hit_unit_id != -1) {
            learner = battle_unit_get_by_battle_id(g_battle_action_learn_on_hit_unit_id);
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_ABILITY_LEARNED, ability_id, unit->stats->battle_id, 0,
                learner->stats->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
            battle_target_move_cursor_to_unit(learner);
            battle_action_set_at_list_active();
        } else {
            g_battle_action_post_action_display_phase++;
        }
        break;
    }
    battle_target_store_cursor_unit_name_and_data();
}

/* Start the post-action reports: restore the units' facings, apply the source
 * unit's EXP and JP rewards (clearing the display values when nothing is
 * awarded), start the earned EXP/JP display and enter the level-up phase of
 * action_init_learn_ability_on_hit. */
void battle_action_handle_post_action_xp_jp_ability(void) {
    battle_unit_t* unit;
    unit_stats_t* stats;

    unit = battle_unit_get_source();
    battle_unit_restore_all_facings_after_attack();
    if (unit != 0) {
        stats = unit->stats;
        if (stats != 0 && battle_unit_apply_action_rewards(stats->battle_id, &unit->strike.action_rewards) == -1) {
            unit->strike.action_rewards.job_level_for_display = 0;
            unit->strike.action_rewards.level_for_display = 0;
            unit->strike.action_rewards.earned_jp = 0;
            unit->strike.action_rewards.earned_experience = 0;
        }
    }
    battle_gfx_init_earned_exp_jp_display(unit);
    g_battle_action_post_action_display_phase = BATTLE_POST_ACTION_DISPLAY_LEVEL_UP;
    battle_target_move_cursor_to_unit(unit);
    battle_action_init_learn_ability_on_hit();
    g_battle_action_post_action = 0;
    g_battle_state_animation_continue_check = 0;
}
