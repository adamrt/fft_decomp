/* Scenario check, free cursor return, Deep Dungeon reload and between-turn events. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/types.h"

/* Run the scenario check for a unit at a turn boundary; when it starts an event,
 * save the game state, enter the event state and return 1. Returns 0 otherwise.
 *
 * With a unit becoming ready, or no turn event, the check may start an event
 * directly (option 0). For a ready ability or an action result the check first
 * only queues the event (option 2), and the event is started only when its finish
 * operation is nonzero and the unit's spritesheet is not one of Ramza's. In every
 * other case, and for the remaining turn event types, the check only closes the
 * menu (option 1) and no event starts. */
s32 battle_state_try_enter_scenario_event(battle_unit_t* unit) {
    s32 result;

    if (g_action_turn_event_type == BATTLE_TURN_EVENT_UNIT_READY
        || g_action_turn_event_type == BATTLE_TURN_EVENT_NONE) {
        result = menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, unit->stats->battle_id, 0, 1);
    } else if (g_action_turn_event_type == BATTLE_TURN_EVENT_ABILITY_READY
        || g_action_turn_event_type == BATTLE_TURN_EVENT_ACTION_RESULT) {
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
        action_set_casting_unit_id_ff();
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

    camera_disable_map_pan();
    casting_unit = battle_unit_get_casting();
    unit = battle_unit_get_source();
    action_set_at_list_active();
    battle_unit_store_entd_flags_without_control_flag(casting_unit);
    if (battle_state_try_enter_scenario_event(unit) == 0) {
        g_battle_game_state = BATTLE_GAME_STATE_OPEN_ACTION_MENUS;
        g_action_post_action = 0;
        camera_rotate_when_unit_tile_not_visible(casting_unit);
    }
}

/* Return to the free-cursor state at animation speed 1: show the panel for the
 * cursor tile and refresh the source unit's control flags from its ENTD flags. */
void battle_state_set_free_cursor(void) {
    battle_unit_t* unit;

    g_main_system_animation_speed = 1;
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_FREE_CURSOR;
    g_battle_menu_help_opening = 0;
    target_store_cursor_unit_name_and_data();
    action_set_at_list_active();
    unit = battle_unit_get_source();
    if (unit != 0) {
        battle_unit_store_entd_flags_without_control_flag(unit);
    }
}

void battle_menu_open_free_cursor_help(void) {
    camera_disable_map_pan();
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
s32 map_check_deep_dungeon_crystal_and_init_state(void) {
    s32 crystal_count;
    s32 previous_count;

    if (g_main_system_frontend_world_result == 4) {
        if (g_map_deep_dungeon_load_in_progress == 0) {
            crystal_count = battle_unit_count_crystals();
            if (crystal_count >= 5) {
                crystal_count = 4;
            }
            previous_count = script_get_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT);
            if (crystal_count != previous_count) {
                script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, crystal_count);
                if (map_start_data_load() != 0) {
                    g_map_deep_dungeon_load_in_progress = 1;
                } else {
                    script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, previous_count);
                }
            }
        }
        return g_map_deep_dungeon_load_in_progress;
    }
    return 0;
}

/* Between turns: when the crystal count calls for a map reload
 * (map_check_deep_dungeon_crystal_and_init_state), enter the Deep Dungeon
 * mesh-load state at animation speed 1. Returns 1 when the state changed. */
s32 map_try_start_deep_dungeon_mesh_load(void) {
    if (map_check_deep_dungeon_crystal_and_init_state() != 0) {
        g_battle_game_state = BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_LOAD;
        g_main_system_animation_speed = 1;
        return 1;
    }

    return 0;
}

/* As map_try_start_deep_dungeon_mesh_load, entering the mesh-finish state
 * instead: the crystal count check made when a unit's post-movement steps end. */
s32 map_refresh_deep_dungeon(void) {
    if (map_check_deep_dungeon_crystal_and_init_state()) {
        g_battle_game_state = BATTLE_GAME_STATE_DEEP_DUNGEON_MESH_FINISH;
        g_main_system_animation_speed = 1;
        return 1;
    }
    return 0;
}

/* Resolves the queued in-between-turn event (mimic, ability cast, traps).
 *
 * The 32-bit queue value's second byte selects the action type (g_action_turn_event_type) and its
 * low byte the unit; BATTLE_TURN_EVENT_NONE means nothing is queued. Unit- and
 * ability-ready events resolve an ability, while action-result events dispatch
 * the queued per-unit result. The meaning of 0x400 remains uncertain. */
void action_run_between_turn_events(void) {
    battle_unit_t* unit;
    s32 action;
    s32 runtime_id;

    action = action_run_between_turn_control(BATTLE_BETWEEN_TURN_MODE_RUN);
    runtime_id = action & 0xff;
    g_action_turn_event_type = action & 0xff00;
    if (g_action_turn_event_type == BATTLE_TURN_EVENT_NONE) {
        return;
    }
    unit = battle_unit_get_by_battle_id(runtime_id);
    if (unit != 0) {
        g_battle_unit_source_runtime_id = unit->runtime_id;
    } else {
        main_system_handle_pointer_exception(12);
    }
    switch (g_action_turn_event_type) {
    case BATTLE_TURN_EVENT_UNIT_READY:
    case BATTLE_TURN_EVENT_ABILITY_READY:
        battle_unit_draw_and_activate_by_runtime_id(unit->runtime_id);
        break;
    case BATTLE_TURN_EVENT_ACTION_RESULT:
        unit->pending_attack_result = action_finalize_attack_and_flag_reactions(runtime_id);
        if (unit->pending_attack_result == -1) {
            battle_unit_find_relocation_tile(runtime_id, &unit->dismount);
            battle_unit_set_map_coords_after_death_dismount(unit);
        }
        break;
    case BATTLE_TURN_EVENT_UNKNOWN_0400:
        unit->pending_attack_result = action_finalize_attack_and_flag_reactions(runtime_id);
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
        if (g_action_turn_event_type != BATTLE_TURN_EVENT_UNKNOWN_0400) {
            target_move_cursor_to_unit(unit);
        }
    }
}

/* Runs any in-between-turn event, then opens the next acting unit's turn. */
void action_check_between_turn_events(void) {
    battle_unit_t* unit;

    if (map_try_start_deep_dungeon_mesh_load() != 0) {
        return;
    }
    action_clear_at_list_id();
    g_main_system_animation_speed = 1;
    action_run_between_turn_events();
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
            && g_action_turn_event_type == BATTLE_TURN_EVENT_UNIT_READY) {
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            main_sound_play_sfx(MAIN_SFX_STATUS_MENU_REQUEST);
            action_open_status_menu_at_list_start();
            action_show_next_at_list_entry();
            battle_menu_set_next_script_action_menus();
            return;
        }
        g_action_post_action = 0;
        if (g_action_turn_event_type == BATTLE_TURN_EVENT_UNIT_READY
            || g_action_turn_event_type == BATTLE_TURN_EVENT_NONE) {
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
        }
        if (battle_state_try_enter_scenario_event(unit) == 0) {
            menu_start_unit_view_thread(UNIT_VIEW_MODE_TILE_INFO, 0xff, 0xff);
            g_battle_game_state = BATTLE_GAME_STATE_CHANGE_TURN;
            ai_init_selected_action();
            g_battle_menu_status_enabled = 1;
        } else {
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
        }
        g_battle_menu_status_requested = 0;
    } else {
        battle_state_set_free_cursor();
    }
}
