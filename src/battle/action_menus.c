/* Idle action menus, the free cursor, the move and wait commands, facing changes, and post-action level-up and message
 * reporting. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/ai_decision.h"
#include "fft/animation.h"
#include "fft/at_list.h"
#include "fft/battle_map.h"
#include "fft/battle_map_load.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/crystal.h"
#include "fft/cursor.h"
#include "fft/effect_script.h"
#include "fft/effect_secondary.h"
#include "fft/help.h"
#include "fft/message.h"
#include "fft/move.h"
#include "fft/music.h"
#include "fft/option.h"
#include "fft/result.h"
#include "fft/reward.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/state.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/target.h"
#include "fft/turn.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "fft/world_map.h"
#include "psx/libgte.h"
#include "psx/types.h"

static s32 g_action_message_count = 0;
action_post_effect_msg_t g_action_messages[ACTION_POST_EFFECT_MSG_CAPACITY] = { 0 };

/* Effect message texts (MENU_SYSTEM_COMMAND_EFFECT_MESSAGE). An ability's message is the base plus
 * the text_display entry of its animation data; the others have fixed ids. */
enum {
    TEXT_ID_BATTLE_MESSAGE_BASE = 0x1800,
    TEXT_ID_BATTLE_MESSAGE_TRAINED = 0x1828,
    TEXT_ID_BATTLE_MESSAGE_POACHED = 0x1838,
    TEXT_ID_BATTLE_MESSAGE_MIME = 0x183a,
    TEXT_ID_BATTLE_MESSAGE_TELEPORT_FAILED = 0x183b,
    TEXT_ID_BATTLE_MESSAGE_TREASURE = 0x183c,
    TEXT_ID_BATTLE_MESSAGE_CRYSTAL = 0x183d,
    TEXT_ID_BATTLE_MESSAGE_JOB_LEVEL_UP = 0x183e,
    TEXT_ID_BATTLE_MESSAGE_LEVEL_UP = 0x183f
};

/* Hand the battle over to an event script: enter the event state at animation
 * speed 2, reset the jump graphics and the event state, disable the status menu
 * and clear the casting and source unit ids (0xff). */
void state_enter_event(void) {
    g_system_vsync_interval = 2;
    g_game_state = GAME_STATE_EVENT;
    unit_hide_all_jumping();
    script_reset_event_state();
    cursor_hide();
    g_menu_status_enabled = 0;
    g_unit_casting_runtime_id = 0xff;
    g_unit_source_runtime_id = 0xff;
}

/* Enter the event state and run the scenario check with no unit (0xff), as the
 * deployment screen does when it finishes. */
void state_enter_event_at_battle_start(void) {
    state_enter_event();
    menu_init_system_function(MENU_SYSTEM_COMMAND_SCENARIO_CHECK, MENU_SCENARIO_CHECK_START, 0xff, 0, 1);
}

/* Run the scenario check for the unit's turn event. A unit-ready or empty event may start a
 * scenario event (START); an ability-ready or action-result event first only tests (TEST). When
 * that test finds a pending event with a finish operation, a Ramza unit runs SKIP and any other
 * unit START (whose result is dropped); otherwise, and for every other turn event, SKIP runs.
 * Returns 1 when the check whose result is kept (for a non-Ramza unit after a successful test,
 * the test itself) dispatched an event, after saving the game state and handing the battle to
 * it. */
s32 event_try_enter_scenario(unit_t* unit) {
    s32 result;

    if (g_turn_event_type == TURN_EVENT_UNIT_READY || g_turn_event_type == TURN_EVENT_NONE) {
        result = menu_init_system_function(
            MENU_SYSTEM_COMMAND_SCENARIO_CHECK, MENU_SCENARIO_CHECK_START, unit->stats->battle_id, 0, 1);
    } else if (g_turn_event_type == TURN_EVENT_ABILITY_READY || g_turn_event_type == TURN_EVENT_ACTION_RESULT) {
        result = menu_init_system_function(
            MENU_SYSTEM_COMMAND_SCENARIO_CHECK, MENU_SCENARIO_CHECK_TEST, unit->stats->battle_id, 0, 1);
        if (result == MENU_SYSTEM_DISPATCH_EVENT && script_get_event_finish_operation() != 0) {
            if (unit->spritesheet_id < SPRITE_SHEET_ID_RAMZA_END) {
                result = menu_init_system_function(
                    MENU_SYSTEM_COMMAND_SCENARIO_CHECK, MENU_SCENARIO_CHECK_SKIP, unit->stats->battle_id, 0, 1);
            } else {
                menu_init_system_function(
                    MENU_SYSTEM_COMMAND_SCENARIO_CHECK, MENU_SCENARIO_CHECK_START, unit->stats->battle_id, 0, 1);
            }
        } else {
            result = menu_init_system_function(
                MENU_SYSTEM_COMMAND_SCENARIO_CHECK, MENU_SCENARIO_CHECK_SKIP, unit->stats->battle_id, 0, 1);
        }
    } else {
        result = menu_init_system_function(
            MENU_SYSTEM_COMMAND_SCENARIO_CHECK, MENU_SCENARIO_CHECK_SKIP, unit->stats->battle_id, 0, 1);
    }
    if (result == MENU_SYSTEM_DISPATCH_EVENT) {
        g_game_state_previous = g_game_state;
        state_enter_event();
        return 1;
    }
    return 0;
}

/* Copy the team flags from the unit's battle stats into its runtime record,
 * dropping UNIT_TEAM_FLAG_PLAYER_CONTROLLED unless its auto-battle setting is
 * manual. A mounted unit's partner gets the same treatment. */
void unit_store_team_flags(unit_t* unit) {
    u8 entd_flags = unit->stats->team_flags;
    u8 mount_entd_flags;
    unit_t* mount;

    unit->team_flags = entd_flags;
    if (unit->stats->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
        unit->team_flags = entd_flags & ~UNIT_TEAM_FLAG_PLAYER_CONTROLLED;
    }
    if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
        mount = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        mount_entd_flags = mount->stats->team_flags;
        mount->team_flags = mount_entd_flags;
        if (mount->stats->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
            mount->team_flags = mount_entd_flags & ~UNIT_TEAM_FLAG_PLAYER_CONTROLLED;
        }
    }
}

/* Return to the source unit's action menus: set the cursor-visible flag, run the
 * scenario check, and unless it starts an event enter the open-action-menus state
 * and turn the camera when the casting unit's tile is hidden. */
void state_enter_open_action_menus(void) {
    unit_t* casting_unit;
    unit_t* unit;

    camera_pan_disable();
    casting_unit = unit_get_casting();
    unit = unit_get_source();
    cursor_show();
    unit_store_team_flags(casting_unit);
    if (event_try_enter_scenario(unit) == 0) {
        g_game_state = GAME_STATE_OPEN_ACTION_MENUS;
        g_state_window_closed = 0;
        camera_rotate_when_unit_tile_not_visible(casting_unit);
    }
}

/* Return to the free-cursor state at vsync interval 1: show the panel for the
 * cursor tile and refresh the source unit's control flags from its ENTD flags. */
void state_enter_free_cursor(void) {
    unit_t* unit;

    g_system_vsync_interval = 1;
    camera_pan_disable();
    g_game_state = GAME_STATE_FREE_CURSOR;
    g_state_in_help = 0;
    cursor_show_hovered_unit_panel();
    cursor_show();
    unit = unit_get_source();
    if (unit != 0) {
        unit_store_team_flags(unit);
    }
}

/* Select in the free cursor: enter the free-cursor help state and start building the help window. */
void state_enter_free_cursor_help(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_FREE_CURSOR_HELP;
    g_state_in_help = 1;
    help_start_menu_thread();
}

/* Reload the map when the number of crystals on the field has changed.
 *
 * Only in a Deep Dungeon battle (WORLD_MAP_RESULT_DEEP_DUNGEON_BATTLE), and not while such a reload is
 * running: the crystal count, capped at 4, is compared with script variable
 * EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, the map state that GNS rows of
 * those maps select on. A new count is stored and a map data load started; the
 * old count is put back when the load cannot start. Returns whether a reload is
 * in progress. */
static s32 battle_map_deep_dungeon_check_crystal_count(void) {
    s32 crystal_count;
    s32 previous_count;

    if (g_system_frontend_world_result == WORLD_MAP_RESULT_DEEP_DUNGEON_BATTLE) {
        if (g_battle_map_deep_dungeon_load_in_progress == 0) {
            crystal_count = unit_count_crystals();
            if (crystal_count >= 5) {
                crystal_count = 4;
            }
            previous_count = script_get_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT);
            if (crystal_count != previous_count) {
                script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, crystal_count);
                if (battle_map_load_start() != 0) {
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
 * (battle_map_deep_dungeon_check_crystal_count), enter the Deep Dungeon
 * mesh-load state at vsync interval 1. Returns 1 when the state changed. */
static s32 battle_map_deep_dungeon_try_start_mesh_load(void) {
    if (battle_map_deep_dungeon_check_crystal_count() != 0) {
        g_game_state = GAME_STATE_DEEP_DUNGEON_MESH_LOAD;
        g_system_vsync_interval = 1;
        return 1;
    }

    return 0;
}

/* As battle_map_deep_dungeon_try_start_mesh_load, entering the mesh-finish state
 * instead: the crystal count check made when a unit's post-movement steps end. */
static s32 battle_map_deep_dungeon_try_start_mesh_finish(void) {
    if (battle_map_deep_dungeon_check_crystal_count()) {
        g_game_state = GAME_STATE_DEEP_DUNGEON_MESH_FINISH;
        g_system_vsync_interval = 1;
        return 1;
    }
    return 0;
}

/* Run the clocktick loop to the next turn event and start it.
 *
 * The loop's result holds a turn_event_e in bits 8-15, stored in
 * g_turn_event_type, and the event's unit id in the low byte;
 * TURN_EVENT_NONE means nothing happened. The unit becomes the source
 * unit. A unit's turn and a charged ability coming due activate the unit; an
 * action result (from a status countdown, poison or regen) is applied to it,
 * and TURN_EVENT_UNKNOWN_0400 also refreshes its display. A Mime event is
 * handled later by the status-execute state. Afterwards the source unit's turn
 * flags are cleared and the cursor moves to it. */
static void turn_take_next_event(void) {
    unit_t* unit;
    s32 action = turn_run_clock(TURN_CLOCK_MODE_RUN);
    s32 unit_id = action & 0xff;

    g_turn_event_type = action & 0xff00;
    if (g_turn_event_type == TURN_EVENT_NONE) {
        return;
    }
    unit = unit_get_by_battle_id(unit_id);
    if (unit != 0) {
        g_unit_source_runtime_id = unit->runtime_id;
    } else {
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    }
    switch (g_turn_event_type) {
    case TURN_EVENT_UNIT_READY:
    case TURN_EVENT_ABILITY_READY:
        unit_draw_and_activate_by_runtime_id(unit->runtime_id);
        break;
    case TURN_EVENT_ACTION_RESULT:
        unit->pending_attack_result = action_apply_target_result(unit_id);
        if (unit->pending_attack_result == -1) {
            unit_find_relocation_tile(unit_id, &unit->dismount);
            unit_dismount_dead_rider(unit);
        }
        break;
    case TURN_EVENT_UNKNOWN_0400:
        unit->pending_attack_result = action_apply_target_result(unit_id);
        if (unit->pending_attack_result == -1) {
            unit_find_relocation_tile(unit_id, &unit->dismount);
            unit_dismount_dead_rider(unit);
        }
        result_prepare_display_by_runtime_id(unit->runtime_id);
        unit_update_display_by_runtime_id(unit->runtime_id);
        break;
    case TURN_EVENT_MIME:
        break;
    case 0x600: /* a label above 0x500 on the error path makes GCC split the upper
                   tree at 0x500 as the target does; nothing compares it, so the
                   original value is not recoverable */
    default:
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_TURN_EVENT);
        break;
    }
    unit = unit_get_source();
    if (unit != 0) {
        unit->turn_action_flags = 0;
        if (g_turn_event_type != TURN_EVENT_UNKNOWN_0400) {
            cursor_move_to_unit(unit);
        }
    }
}

/* Between turns: unless a Deep Dungeon map reload takes over, run the clocktick
 * loop to the next turn event and open it for its unit.
 *
 * The unit becomes the casting unit. When the player asked for the status menu
 * (Triangle) during a turn the player does not control, a unit-ready event
 * opens the status menu at the start of the AT list instead. Otherwise the
 * scenario check runs and, unless it starts an event, the change-turn state
 * begins with the tile panel and the status menu enabled. Without battle data
 * the free cursor returns. */
void turn_start_next_event(void) {
    unit_t* unit;

    if (battle_map_deep_dungeon_try_start_mesh_load() != 0) {
        return;
    }
    at_list_reset_cursor();
    g_system_vsync_interval = 1;
    turn_take_next_event();
    unit = unit_get_source();
    if (unit == 0) {
        return;
    }
    g_unit_casting_runtime_id = unit->runtime_id;
    unit_store_team_flags(unit);
    if (unit->stats != 0) {
        command_reset_unit_record(unit->stats->battle_id);
        unit_store_team_flags(unit); /* again, as retail does */
        if (g_menu_status_requested != 0 && !(unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED)
            && g_turn_event_type == TURN_EVENT_UNIT_READY) {
            unit_update_display_by_runtime_id(unit->runtime_id);
            sound_play_sfx(SOUND_SFX_STATUS_MENU_REQUEST);
            menu_status_open();
            at_list_show_next_entry();
            state_enter_open_action_menus();
            return;
        }
        g_state_window_closed = 0;
        if (g_turn_event_type == TURN_EVENT_UNIT_READY || g_turn_event_type == TURN_EVENT_NONE) {
            unit_update_display_by_runtime_id(unit->runtime_id);
        }
        if (event_try_enter_scenario(unit) == 0) {
            unit_view_start(UNIT_VIEW_MODE_TILE_INFO, 0xff, 0xff);
            g_game_state = GAME_STATE_CHANGE_TURN;
            ai_init_selected_action();
            g_menu_status_enabled = 1;
        } else {
            unit_update_display_by_runtime_id(unit->runtime_id);
        }
        g_menu_status_requested = 0;
    } else {
        state_enter_free_cursor();
    }
}

/* Enter the status-execute state for the source unit. A Mime turn event shows the Mime effect
 * message for one second and kneels the unit; otherwise a queued Crystal or Treasure
 * transformation shows its message for three seconds (any other result closes the window), and
 * the unit's queued statuses and post-action display are applied. */
void state_enter_status_execute(void) {
    unit_t* unit;
    u32 statuses;

    camera_pan_disable();
    g_game_state = GAME_STATE_STATUS_EXECUTE;
    unit = unit_get_source();
    cursor_hide();
    g_state_window_closed = 0;
    if (g_turn_event_type == TURN_EVENT_MIME) {
        message_set_duration(60);
        menu_init_system_function(MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_MIME,
            unit->stats->battle_id, unit->stats->battle_id, 1);
        animation_force_facing(ANIMATION_KNEEL, unit->facing, unit);
        return;
    }
    message_set_duration(180);
    statuses = unit->statuses_to_add_5_6;
    if (statuses & UNIT_STATUS_CRYSTAL) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_CRYSTAL,
            unit->stats->battle_id, unit->stats->battle_id, 1);
    } else if (statuses & UNIT_STATUS_TREASURE) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_TREASURE,
            unit->stats->battle_id, unit->stats->battle_id, 1);
    } else {
        menu_init_system_function(MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
    unit_update_display_by_runtime_id(unit->runtime_id);
    result_prepare_display_by_runtime_id(unit->runtime_id);
}

/* Enter the AI-command state and restart the source unit's command. The
 * unit is written before the null test that guards the panel. */
static void state_enter_ai_command(void) {
    unit_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_AI_COMMAND;
    unit = unit_get_source();
    unit->state_frame_counter = 0;
    unit->command_ready = 0;
    target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 0);
    if (unit != 0 && unit->stats != 0) {
        unit_view_start(UNIT_VIEW_MODE_UNIT_SUMMARY, unit->stats->battle_id, 0);
    }
}

/* Enter the highlight-units state: tint every unit by its team, save the cursor
 * input mode and switch to the free cursor with map panning. */
void state_enter_highlight_units(void) {
    camera_pan_enable();
    g_game_state = GAME_STATE_HIGHLIGHT_UNITS;
    g_cursor_input_mode_saved = g_cursor_input_mode;
    g_cursor_input_mode = CURSOR_INPUT_FREE_CURSOR;
    sprite_palette_tint_all_by_team();
    cursor_hide();
}

/* Open the source unit's own action menu: the unit becomes the casting unit, the
 * cursor returns to it, the idling-action-menus state begins with the unit
 * summary panel, and the menu for its remaining move and action is built. */
void menu_action_open_active_unit(void) {
    unit_t* unit;

    camera_pan_disable();
    unit = unit_get_source();
    g_unit_casting_runtime_id = unit->runtime_id;
    cursor_move_to_unit(unit);
    g_game_state = GAME_STATE_IDLING_ACTION_MENUS;

    if (unit != 0 && unit->stats != 0) {
        unit_view_start(UNIT_VIEW_MODE_UNIT_SUMMARY, unit->stats->battle_id, 0);
    }
    command_open_menu(menu_action_get_menu_id(unit->stats->battle_id));
}

/* Open the idle action menu of the unit under the cursor when it is not the
 * acting unit, or while the status menu is open; there the player and other
 * unit menus switch to their inspect variants. An empty tile does nothing. */
void menu_action_open_non_active_unit(void) {
    unit_t* unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_z);
    unit_stats_t* stats;
    s32 menu_id;

    if (unit == 0) {
        return;
    }
    camera_pan_disable();
    stats = unit->stats;
    g_game_state = GAME_STATE_IDLING_ACTION_MENUS;
    unit_view_start(UNIT_VIEW_MODE_UNIT_SUMMARY, stats->battle_id, 0);
    menu_id = menu_action_get_menu_id(unit->stats->battle_id);
    if (g_menu_status_open != 0) {
        if (menu_id == MENU_COMMAND_ID_PLAYER_UNIT) {
            menu_id = MENU_COMMAND_ID_PLAYER_UNIT_INSPECT;
        } else if (menu_id == MENU_COMMAND_ID_OTHER_UNIT) {
            menu_id = MENU_COMMAND_ID_OTHER_UNIT_INSPECT;
        }
    }
    command_open_menu(menu_id);
}

/* Open the action menu for the unit on the cursor tile, which becomes the casting
 * unit.
 *
 * The source unit, with the status menu closed, gets its own turn menu when the
 * player controls it and otherwise restarts the AI-command state. Any
 * other unit gets the non-active-unit menu. An empty tile does nothing. */
void menu_action_dispatch(void) {
    unit_t* unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_z);

    if (unit == 0) {
        return;
    }
    g_unit_casting_runtime_id = unit->runtime_id;
    if (g_menu_status_open == 0 && unit->runtime_id == g_unit_source_runtime_id) {
        if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
            menu_action_open_active_unit();
        } else {
            state_enter_ai_command();
        }
    } else {
        menu_action_open_non_active_unit();
    }
}

/* Select in the idle action menus: enter the action-help state and start building the help window. */
void state_enter_action_help_menu(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_ACTION_HELP_MENU;
    g_state_in_help = 1;
    help_start_menu_thread();
}

/* Cross on a unit in the free cursor: show the casting unit's reachable tiles and enter the
 * display-move-area state with map panning, saving the cursor input mode for the Cross press that
 * ends it. */
void state_enter_display_move_area(void) {
    unit_t* unit;
    unit_stats_t* stats;
    s32 previous_mode;

    camera_pan_enable();
    g_game_state = GAME_STATE_DISPLAY_MOVE_AREA;
    unit = unit_get_casting();
    stats = unit->stats;
    move_range_build(stats->battle_id, unit->map_x, unit->map_y, unit->map_z);
    target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_SHOW_MOVE, 1);
    previous_mode = g_cursor_input_mode;
    g_cursor_input_mode = CURSOR_INPUT_FREE_CURSOR;
    g_cursor_input_mode_saved = previous_mode;
    sound_play_sfx(SOUND_SFX_CONFIRM);
}

/* Triangle in the free cursor, or the end of the mini-menu help: enter the mini-menu state and
 * start the mini menu. */
void state_enter_mini_menu(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_MINI_MENU;
    mini_menu_start();
}

/* Leave the view of a unit picked from the AT list: enter the mini-menu state,
 * remove the tile marks and release OPTION.OUT's AT list thread, which waits on
 * g_at_list_selection_finished. */
void state_return_to_mini_menu(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_MINI_MENU;
    target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 3);
    at_list_finish_selection();
}

/* Select in the mini menu: enter the mini-menu help state and start building the help window. */
void state_enter_mini_menu_help(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_MINI_MENU_HELP;
    g_state_in_help = 1;
    help_start_menu_thread();
}

/* Show the AT-list entry picked in the mini menu: put the cursor on its unit or, for a
 * charging action, on that action's target with the target tiles marked. */
void at_list_show_entry(s32 at_index) {
    s32 id;
    unit_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_AT_LIST_INSPECTION;
    id = at_list_get_entry_unit_id(at_index);
    if (id >= 0) {
        unit = unit_get_by_battle_id(id & AT_LIST_ENTRY_BATTLE_ID_MASK);
        if ((id & AT_LIST_ENTRY_CHARGED_ACTION) != 0) {
            cursor_move_to_action_target(unit);
            target_area_set_for_action(&unit->stats->action_command);
            target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_SHOW_TARGETS, 3);
        } else {
            cursor_move_to_unit(unit);
            target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 0);
        }
    }
    cursor_show_hovered_unit_panel();
    cursor_show();
}

/* Enter the unit-move state and show the source unit's targeting prompt. */
void state_enter_unit_move(void) {
    unit_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_UNIT_MOVE;
    unit = unit_get_source();
    menu_init_system_function(MENU_SYSTEM_COMMAND_TARGETING_PROMPT, MENU_TARGETING_PROMPT_DEFAULT,
        unit->stats->battle_id, 0, unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Enter the move-range-exception state, reached when the casting unit has no
 * reachable tile: opens the source unit's targeting prompt with
 * MENU_TARGETING_PROMPT_NO_MOVE_RANGE. */
static void state_enter_move_range_exception(void) {
    unit_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_MOVE_RANGE_EXCEPTION;
    unit = unit_get_source();
    menu_init_system_function(MENU_SYSTEM_COMMAND_TARGETING_PROMPT, MENU_TARGETING_PROMPT_NO_MOVE_RANGE,
        unit->stats->battle_id, 0, unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Return to choosing a move destination: recompute the source unit's reachable
 * tiles, show them and enter the select-move-tile state with the cursor unit's
 * panel. With no reachable tile the move-range exception state is entered
 * instead. The cursor-visible flag follows whether the player controls the unit. */
void state_enter_select_move_tile(void) {
    unit_t* unit;
    unit_t* casting;

    camera_pan_disable();
    unit = unit_get_source();
    casting = unit_get_casting();
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        cursor_show();
    } else {
        cursor_hide();
    }

    if (move_range_build(casting->stats->battle_id, casting->map_x, casting->map_y, casting->map_z) > 0) {
        target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_SHOW_MOVE, 1);
        g_game_state = GAME_STATE_SELECT_MOVE_TILE;
        casting->state_frame_counter = 0;
        cursor_show_hovered_unit_panel();
    } else {
        state_enter_move_range_exception();
    }
}

/* Enter the illegal-move state and show move feedback that rejects the chosen
 * destination. */
void state_enter_illegal_move_menu(void) {
    unit_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_ILLEGAL_MOVE_MENU;
    unit = unit_get_source();
    menu_init_system_function(MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, MENU_MOVE_FEEDBACK_ILLEGAL_MOVE,
        unit->stats->battle_id, 0, unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Enter the unit-moving-setup state for the chosen destination and start the source unit's move
 * feedback: confirm a pending mount or a step onto a crystal, otherwise close. A teleport (path count 0xfe or 0xff, the
 * u8 count + 2 < 2 test) asks for move confirmation instead when navigation messages are on. */
void state_enter_unit_moving_setup(void) {
    unit_t* source;
    unit_t* casting;
    unit_t* at_tile;
    u8 path_count;

    camera_pan_disable();
    g_game_state = GAME_STATE_UNIT_MOVING_SETUP;
    source = unit_get_source();
    casting = unit_get_casting();

    if (casting->path.pending_mount & MOVE_PENDING_MOUNT_SET) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, MENU_MOVE_FEEDBACK_MOUNT, source->stats->battle_id,
            0, source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        return;
    }
    path_count = casting->path.count + 2;
    if (path_count < 2) {
        if (g_game_options.fields.navigation_messages != GAME_OPTION_ON) {
            menu_init_system_function(MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, MENU_MOVE_FEEDBACK_CLOSE,
                source->stats->battle_id, 0, source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
            return;
        }
        menu_init_system_function(MENU_SYSTEM_COMMAND_MOVE_CONFIRM, 0, source->stats->battle_id, 0,
            source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        return;
    }
    at_tile = unit_get_crystal_or_treasure_at_map_coords(g_cursor_x, g_cursor_y, g_cursor_z);
    if (at_tile != 0) {
        if (at_tile->status_flags_5_6 & UNIT_STATUS_CRYSTAL) {
            menu_init_system_function(MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, MENU_MOVE_FEEDBACK_CRYSTAL,
                source->stats->battle_id, 0, source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
            return;
        }
    }
    menu_init_system_function(MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, MENU_MOVE_FEEDBACK_CLOSE, source->stats->battle_id, 0,
        source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Save the unit's tile, facing, mount link and stood-on unit before it moves;
 * move_restore_previous_state restores them. Every field is read
 * before any is stored, as the retail code orders them. */
static void move_save_previous_state(unit_t* unit) {
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
void state_enter_unit_moving(void) {
    unit_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_UNIT_MOVING;
    cursor_show_hovered_unit_panel();
    unit = unit_get_casting();
    move_save_previous_state(unit);
    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        unit_t* mount = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (mount != 0) {
            move_save_previous_state(mount);
        }
    }
    unit->step_speed = 0x2000;
    unit->movement_path_offset = 0;
    g_state_animation_continue_check = 0;
    cursor_hide();
}

/* Enter the move-confirm state once the unit has stopped moving and show the
 * matching prompt, with a one-second message time.
 *
 * A failed Teleport shows its effect message; a successful one the move
 * confirmation. Other movement tints the unit by team and asks for confirmation,
 * silently when navigation messages are off. The cursor-visible flag follows
 * whether the player controls the unit. */
void state_enter_move_confirm_menu(void) {
    unit_t* source;
    unit_t* caster;

    camera_pan_disable();
    g_game_state = GAME_STATE_MOVE_CONFIRM_MENU;
    source = unit_get_source();
    caster = unit_get_casting();
    message_set_duration(60);
    if (caster->path.flags & MOVE_EFFECTIVE_TELEPORT) {
        if (caster->last_path_count == MOVE_PATH_TELEPORT_FAILED) {
            menu_init_system_function(MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_TELEPORT_FAILED,
                source->stats->battle_id, 0, 1);
        } else {
            menu_init_system_function(MENU_SYSTEM_COMMAND_MOVE_CONFIRM, 0, source->stats->battle_id, 0, 0);
        }
        caster->last_path_count = 0;
    } else {
        if (g_game_options.fields.navigation_messages != GAME_OPTION_ON) {
            sprite_palette_tint_by_team(source, 2);
            menu_init_system_function(MENU_SYSTEM_COMMAND_MOVE_CONFIRM_SILENT, 0, source->stats->battle_id, 0,
                source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            sprite_palette_tint_by_team(source, 2);
            menu_init_system_function(MENU_SYSTEM_COMMAND_MOVE_CONFIRM, 0, source->stats->battle_id, 0,
                source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        }
    }
    if (source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) {
        cursor_show();
    } else {
        cursor_hide();
    }
}

/* Enter the after-command state at vsync interval 1: clear the window-closed flag, put the cursor
 * back on the source unit and close the system function window. */
void state_enter_after_command(void) {
    g_system_vsync_interval = 1;
    g_game_state = GAME_STATE_AFTER_COMMAND;
    g_state_window_closed = 0;
    cursor_move_to_unit(unit_get_source());
    menu_init_system_function(MENU_SYSTEM_COMMAND_CLOSE, 0, 0, 0, 0);
}

/* Turn a unit to one of the four cardinal facings and restart its animation.
 * A diagonal or any other angle leaves it as it is. */
void unit_facing_set_cardinal(unit_t* unit, s32 facing) {
    switch (facing) {
    case UNIT_FACING_NORTH:
        if (unit->facing == UNIT_FACING_NORTH) {
            return;
        }
        unit->facing = UNIT_FACING_NORTH;
        break;
    case UNIT_FACING_SOUTH:
        if (unit->facing == UNIT_FACING_SOUTH) {
            return;
        }
        unit->facing = UNIT_FACING_SOUTH;
        break;
    case UNIT_FACING_WEST:
        if (unit->facing == UNIT_FACING_WEST) {
            return;
        }
        unit->facing = UNIT_FACING_WEST;
        break;
    case UNIT_FACING_EAST:
        if (unit->facing == UNIT_FACING_EAST) {
            return;
        }
        unit->facing = UNIT_FACING_EAST;
        break;
    case UNIT_FACING_SOUTH_WEST:
    case UNIT_FACING_NORTH_WEST:
    case UNIT_FACING_NORTH_EAST:
    case UNIT_FACING_SOUTH_EAST:
    default:
        return;
    }
    animation_set_for_status(unit);
}

/* Report a level up recorded in the unit's rewards: its message for three
 * seconds, the level-up animation and effect, the cursor on the unit and the
 * level-up tune. Returns 1 after reporting one, otherwise 0. */
static s32 reward_report_level_up(unit_t* unit) {
    if (unit->strike.action_rewards.level_for_display != 0) {
        u8 battle_id;

        message_set_duration(180);
        battle_id = unit->stats->battle_id;
        menu_init_system_function(
            MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_LEVEL_UP, battle_id, battle_id, 1);
        animation_set_level_up(unit);
        cursor_move_to_unit(unit);
        effect_secondary_set_level_up(unit);
        music_play_tune(MUSIC_TUNE_LEVEL_UP);
        return 1;
    }
    return 0;
}

/* Report a job level up recorded in the unit's rewards: its message for three
 * seconds, the level-up animation and effect, the cursor on the unit and the job
 * level-up tune. Returns 1 after reporting one, otherwise 0. */
static s32 reward_report_job_level_up(unit_t* unit) {
    if (unit->strike.action_rewards.job_level_for_display != 0) {
        u8 battle_id;

        message_set_duration(180);
        battle_id = unit->stats->battle_id;
        menu_init_system_function(
            MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_JOB_LEVEL_UP, battle_id, battle_id, 1);
        animation_set_level_up(unit);
        cursor_move_to_unit(unit);
        effect_secondary_set_level_up(unit);
        music_play_tune(MUSIC_TUNE_JOB_LEVEL_UP);
        return 1;
    }
    return 0;
}

/* Start the next post-movement step selected by the move_post_event_e flags in
 * g_move_post_events.
 *
 * Handles, in priority order, the crystal or treasure pickup, the movement
 * ability's EXP/JP and level-up reports, a found rare or common item, a tile
 * trap, a canceled charge and the post-action displays for the source unit or
 * its mount partner. Returns 1 when a step was started; otherwise refreshes
 * both units, re-enters the after-command state unless the Deep Dungeon map
 * refresh takes over, and returns 0. */
s32 move_start_next_post_event(void) {
    unit_t* source = unit_get_source();
    unit_t* casting = unit_get_casting();
    unit_t* target;
    unit_stats_t* stats;
    s32 item;
    s32 animation;
    s32 trap;

    menu_init_system_function(MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
    g_game_state = GAME_STATE_POST_MOVE_EVENTS;
    g_state_window_closed = 0;
    casting->state_frame_counter = 0;
    if (g_move_post_events & MOVE_POST_EVENT_CRYSTAL_OR_TREASURE) {
        target = unit_get_crystal_or_treasure_at_map_coords(source->map_x, source->map_y, source->map_z);
        /* -1 for a crystal, otherwise the treasure's item id. */
        item = crystal_resolve_pickup(source->stats->battle_id);
        if (item == -1) {
            animation = ANIMATION_RAISE_HANDS;
        } else {
            sprite_display_store_item(source, item);
            animation = ANIMATION_HOLD_ITEM;
        }
        animation_force_facing(animation, source->facing, source);
        if (item == -1) {
            /* A player-controlled unit of the player team picks from the
             * crystal menu; any other unit takes the HP/MP restore at once. */
            stats = source->stats;
            if (!(stats->initial_team_flags & UNIT_TEAM_MASK)
                && (stats->initial_team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED)) {
                menu_init_system_function(
                    MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, stats->battle_id, stats->battle_id, 0, 1);
            } else {
                menu_init_system_function(
                    MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, stats->battle_id, stats->battle_id, 0, 0);
                crystal_apply_choice(source->stats, CRYSTAL_RESTORE_HP_MP);
                action_apply_target_result(source->stats->battle_id);
                result_prepare_display(source);
                unit_update_display_by_runtime_id(source->runtime_id);
            }
        } else {
            if (!(source->stats->initial_team_flags & UNIT_TEAM_MASK)) {
                menu_init_system_function(
                    MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, source->stats->battle_id, source->stats->battle_id, 0, 1);
            } else {
                menu_init_system_function(
                    MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, source->stats->battle_id, source->stats->battle_id, 0, 0);
            }
            if ((source->stats->initial_team_flags & UNIT_TEAM_MASK)
                || !(source->stats->initial_team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED)) {
                crystal_apply_choice(source->stats, CRYSTAL_TAKE_TREASURE);
            }
        }
        unit_fade_out_start_red(target->runtime_id);
        target->depth_height_offset
            = 0x24 - (unit_height_calculate_from_world_coords(source) - target->world_coords.vy);
        sound_play_sfx_on_free_channel(SOUND_SFX_COMPLETE);
        return 1;
    }
    if (g_move_post_events & MOVE_POST_EVENT_MOVEMENT_BENEFIT) {
        switch (g_reward_report_phase) {
        case REWARD_REPORT_LEVEL_UP:
            move_init_ability_benefit(casting->stats);
            reward_apply_result_and_collect(casting->stats, &casting->strike.action_rewards);
            result_prepare_display(casting);
            result_init_earned_exp_jp_display(casting);
            reward_report_level_up(casting);
            return 1;
        case REWARD_REPORT_JOB_LEVEL_UP:
            reward_report_job_level_up(casting);
            return 1;
        }
        return 1;
    }
    if (g_move_post_events & MOVE_POST_EVENT_ITEM_FOUND) {
        g_move_find_display_item = *move_find_determine_item(casting->stats);
        sprite_display_store_item(source, g_move_find_display_item.item_id);
        animation_force_facing(ANIMATION_FOUND_ITEM, source->facing, source);
        if (!(source->team_flags & UNIT_TEAM_MASK)) {
            menu_init_system_function(
                MENU_SYSTEM_COMMAND_MOVE_FIND_ITEM, g_move_find_display_item.item_id, source->stats->battle_id, 0, 1);
            music_play_tune(MUSIC_TUNE_ITEM_FOUND);
            return 1;
        }
        menu_init_system_function(
            MENU_SYSTEM_COMMAND_MOVE_FIND_ITEM, g_move_find_display_item.item_id, source->stats->battle_id, 0, 0);
        return 1;
    }
    if (g_move_post_events & MOVE_POST_EVENT_TRAP_TRIGGERED) {
        trap = move_find_prepare_trap_result(casting->stats);
        if ((source->pending_attack_result = action_apply_target_result(source->stats->battle_id)) == -1) {
            unit_find_relocation_tile(source->stats->battle_id, &source->dismount);
            unit_dismount_dead_rider(source);
        }
        result_prepare_display(source);
        action_start_target_response(0, source);
        effect_secondary_set_trap(source);
        menu_init_system_function(MENU_SYSTEM_COMMAND_TRAP_RESULT, trap, source->stats->battle_id, 0, 1);
        ai_record_triggered_trap_coords(casting->map_x, casting->map_z, casting->map_y);
        return 1;
    }
    if (g_move_post_events & MOVE_POST_EVENT_CHARGING_CANCEL) {
        status_check_charging_charge(casting->stats, 1);
        menu_init_system_function(MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
        return 1;
    }
    if (g_move_post_events & MOVE_POST_EVENT_SOURCE_DISPLAY) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
        result_prepare_display_by_runtime_id(source->runtime_id);
        unit_update_display_by_runtime_id(source->runtime_id);
        return 1;
    }
    if (g_move_post_events & MOVE_POST_EVENT_MOUNT_STATUS_CHANGE) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
        if (source->mount_state != UNIT_MOUNT_STATE_NONE) {
            target = unit_get_by_runtime_id(source->mount_partner_runtime_id);
            if (target != 0) {
                result_prepare_display_by_runtime_id(target->runtime_id);
                unit_update_display_by_runtime_id(target->runtime_id);
            }
        }
        return 1;
    }
    unit_update_display_by_runtime_id(source->runtime_id);
    unit_update_display_by_runtime_id(casting->runtime_id);
    if (battle_map_deep_dungeon_try_start_mesh_finish() == 0) {
        state_enter_after_command();
    }
    return 0;
}

/* Stop the game flow after the casting unit's move and start the post-movement
 * steps.
 *
 * move_get_post_events builds the step flags in
 * g_move_post_events (0 for a unit without battle data), and the
 * post-action display starts at the level-up report. When any step is pending the
 * map cursor and its tile glow are hidden. */
void move_start_post_events(void) {
    unit_stats_t* stats;

    camera_pan_disable();
    stats = unit_get_casting()->stats;
    if (stats != 0) {
        g_move_post_events = move_get_post_events(stats);
    } else {
        g_move_post_events = 0;
    }
    g_reward_report_phase = REWARD_REPORT_LEVEL_UP;
    move_start_next_post_event();
    if (g_move_post_events != 0) {
        cursor_hide();
    }
    g_state_animation_continue_check = 0;
}

/* Enter the wait-direction state for the source unit, starting from its current facing. A rider
 * first turns to its mount's facing; an AI unit picks its wait facing now. */
void state_enter_wait_direction(void) {
    unit_t* mounted_unit;
    unit_t* unit;
    s32 facing;

    camera_pan_disable();
    unit = unit_get_source();
    facing = unit->facing;
    g_game_state = GAME_STATE_WAIT_DIRECTION;
    g_state_wait_direction_facing = facing;
    unit->state_frame_counter = 0;
    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        mounted_unit = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (mounted_unit != 0) {
            unit_facing_set_cardinal(unit, mounted_unit->facing);
            if (unit->stats != 0) {
                unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z,
                    (mounted_unit->facing / UNIT_FACING_QUARTER_TURN));
            }
        }
    }
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) == 0) {
        unit->command_state.cursor.facing_hint = ai_facing_choose_for_wait();
    }
}

/* Open the Wait menu for the source unit, or skip it.
 *
 * A scenario check that finds a pending event whose finish operation is nonzero
 * starts that event instead. A unit whose turn status blocks the Wait menu goes
 * straight to the between-turn events. Otherwise the wait-menu state opens with
 * the cursor unit's panel. The cursor-visible flag follows whether the player
 * controls the unit. */
void state_enter_wait_menu(void) {
    unit_t* unit;

    camera_pan_disable();
    unit = unit_get_source();
    if (menu_init_system_function(
            MENU_SYSTEM_COMMAND_SCENARIO_CHECK, MENU_SCENARIO_CHECK_TEST, unit->stats->battle_id, 0, 1)
            == MENU_SYSTEM_DISPATCH_EVENT
        && script_get_event_finish_operation() != 0) {
        g_game_state_previous = g_game_state;
        menu_init_system_function(
            MENU_SYSTEM_COMMAND_SCENARIO_CHECK, MENU_SCENARIO_CHECK_START, unit->stats->battle_id, 0, 1);
        state_enter_event();
        return;
    }
    if (turn_calculate_status_flags(unit->stats) & TURN_STATUS_BLOCKS_WAIT_MENU_MASK) {
        turn_start_next_event();
    } else {
        g_game_state = GAME_STATE_WAIT_MENU;
        cursor_show_hovered_unit_panel();
        menu_init_system_function(MENU_SYSTEM_COMMAND_WAIT_MENU, 0, unit->stats->battle_id, 0,
            unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
    }
    if (unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) {
        cursor_show();
    } else {
        cursor_hide();
    }
}

/* Face a unit toward the center of the cursor tile; a null unit, or one already
 * on that center, keeps its facing.
 *
 * Subtracting half a tile from the unit instead of adding it to the tile edge
 * gives the same vector. ratan2 returns 4096 per turn: taking off an eighth of
 * a turn and masking to quarter turns picks one of the four cardinal facings. */
void unit_facing_turn_to_cursor(unit_t* unit) {
    VECTOR difference;
    s32 unit_x;
    s32 unit_y;

    if (unit != 0) {
        s32 cursor_x = g_cursor_x;
        s32 cursor_y = g_cursor_y;
        s32 tile_x;
        s32 tile_y;
        difference.vz = 0;
        difference.vx = 0;
        tile_x = cursor_x * BATTLE_MAP_TILE_SIZE;
        unit_x = unit->world_coords.vx - BATTLE_MAP_TILE_SIZE / 2;
        difference.vx = tile_x - unit_x;
        tile_y = cursor_y * BATTLE_MAP_TILE_SIZE;
        unit_y = unit->world_coords.vz - BATTLE_MAP_TILE_SIZE / 2;
        difference.vz = tile_y - unit_y;
        if (difference.vx != 0 || difference.vz != 0) {
            unit->facing = (ratan2(-difference.vz, difference.vx) - 0x200) & 0xC00;
        }
    }
}

/* If the unit has no saved facing yet (-1), save its current facing in
 * attack_facing so unit_facing_restore_after_attack can turn it back
 * once the action is over. The retail code reads attack_facing with a signed
 * `lh`, hence the s16 access. */
static void unit_facing_save_before_attack(unit_t* unit) {
    s16 facing;
    char unused[16]; /* the retail stack frame has 16 unused bytes */

    if (unit != 0) {
        if (unit->attack_facing == -1) {
            facing = unit->facing;
            unit->attack_facing = facing;
        }
    }
}

/* Turn a unit to face a target unit, or its action's target.
 *
 * Without an explicit target, a tile-targeted action aims at the center of the
 * target tile and other actions at the target unit. Units whose statuses
 * prevent turning (UNIT_STATUS_PREVENT_FACING_MASK) or whose strike
 * carries ABILITY_ID_REACTION_REFLECT keep their facing. Otherwise the old
 * facing is saved for the restore, the new one snaps to a quadrant and is
 * stored in the battle stats in quarter turns, and a mounted pair's partner
 * turns with the unit. The second unit_set_tile_position call repeats
 * the unit's own id rather than the partner's. */
void unit_facing_turn_to_target(unit_t* unit, unit_t* target) {
    VECTOR delta;
    unit_t* other;

    if (unit == 0) {
        return;
    }
    delta.vz = 0;
    delta.vx = 0;
    if (target != 0) {
        delta.vx = target->world_coords.vx - unit->world_coords.vx;
        delta.vz = target->world_coords.vz - unit->world_coords.vz;
    } else if (unit->stats->action_command.targeting_type == TARGET_TYPE_TILE) {
        delta.vx = unit->stats->action_command.target_position.vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2)
            - unit->world_coords.vx;
        delta.vz = unit->stats->action_command.target_position.vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2)
            - unit->world_coords.vz;
    } else {
        other = unit_get_by_battle_id(unit->stats->action_command.target_id);
        if (other != 0) {
            delta.vx = other->world_coords.vx - unit->world_coords.vx;
            delta.vz = other->world_coords.vz - unit->world_coords.vz;
        }
    }
    if (unit->status_flags_1_4 & UNIT_STATUS_PREVENT_FACING_MASK) {
        return;
    }
    if (unit->strike.reaction_ability_id == ABILITY_ID_REACTION_REFLECT) {
        return;
    }
    if (delta.vx == 0 && delta.vz == 0) {
        return;
    }
    unit_facing_save_before_attack(unit);
    unit->facing = (ratan2(-delta.vz, delta.vx) - 0x200) & 0xc00;
    unit_set_tile_position(
        unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, unit->facing / UNIT_FACING_QUARTER_TURN);
    if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
        unit_get_by_runtime_id(unit->mount_partner_runtime_id)->facing = unit->facing;
        unit_set_tile_position(
            unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, unit->facing / UNIT_FACING_QUARTER_TURN);
    }
}

/* Turns a living unit back to the facing saved in attack_facing (-1 for none) and
 * clears the saved value; a dead unit only has it cleared.
 *
 * The battle stats take the facing in quarter turns through
 * unit_set_tile_position. For a mounted pair the partner record's facing
 * is copied too, without a null test, and the tile position is stored a second
 * time under the unit's own id. */
static void unit_facing_restore_after_attack(unit_t* unit) {
    unit_t* mount;
    s16 facing;
    s32 unused[4]; /* the retail stack frame has 16 unused bytes */

    if (unit != 0) {
        if ((unit->status_flags_1_4 & UNIT_STATUS_DEAD) == 0) {
            /* The facing 16-bit values are read signed (lh) here. */
            facing = unit->attack_facing;
            if (facing != -1) {
                unit->facing = facing;
                unit_set_tile_position(
                    unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, facing / UNIT_FACING_QUARTER_TURN);
                /* Stored as a signed 16-bit value (li -1, not ori 0xffff). */
                unit->attack_facing = -1;
                if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
                    mount = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                    mount->facing = unit->facing;
                    unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z,
                        unit->facing / UNIT_FACING_QUARTER_TURN);
                }
            }
        } else {
            unit->attack_facing = -1;
        }
    }
}

/* Restore the facing each unit in the attacker's target list had before the
 * attack. */
void unit_facing_restore_targets_after_attack(unit_t* unit) {
    s32 i;
    unit_t* target;

    if (unit->strike.target_count != 0) {
        for (i = 0; i < unit->strike.target_count; i++) {
            target = unit_get_by_battle_id(unit->strike.target_list[i]);
            if (target != 0) {
                unit_facing_restore_after_attack(target);
            }
        }
    }
}

/* unit_facing_restore_after_attack for every runtime id 0..15. */
static void unit_facing_restore_all_after_attack(void) {
    s32 i;

    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        unit_t* unit = unit_get_by_runtime_id(i);

        if (unit != 0) {
            unit_facing_restore_after_attack(unit);
        }
    }
}

/* Enter the continue-turn state at vsync interval 1 and restart the source unit's
 * state frame counter. */
void state_enter_continue_turn(void) {
    unit_t* unit;

    g_system_vsync_interval = 1;
    g_game_state = GAME_STATE_CONTINUE_TURN;
    unit = unit_get_source();
    if (unit != 0) {
        unit->state_frame_counter = 0;
    }
}

/* Ends a unit's command: the cursor returns to the unit, its display is updated
 * and the game enters the after-command state. */
void action_end_command(unit_t* unit) {
    cursor_move_to_unit(unit);
    unit_update_display_by_runtime_id(unit->runtime_id);
    state_enter_after_command();
}

/*
 * Enter the targeting-prompt state after the ability preview: the
 * targeting prompt with MENU_TARGETING_PROMPT_NO_TARGET when the preview marked
 * no tile (TARGET_PANELS_NONE), MENU_TARGETING_PROMPT_DEFAULT otherwise. The cursor-visible flag then follows whether
 * the player controls the unit.
 */
void state_enter_targeting_prompt(void) {
    unit_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_TARGETING_PROMPT;
    unit = unit_get_source();
    if (unit->ability_preview_phase == TARGET_PANELS_NONE)
        menu_init_system_function(MENU_SYSTEM_COMMAND_TARGETING_PROMPT, MENU_TARGETING_PROMPT_NO_TARGET,
            unit->stats->battle_id, 0, unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
    else
        menu_init_system_function(MENU_SYSTEM_COMMAND_TARGETING_PROMPT, MENU_TARGETING_PROMPT_DEFAULT,
            unit->stats->battle_id, 0, unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
    if (unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED)
        cursor_show();
    else
        cursor_hide();
}

/* Enter the action-execute state for the casting unit, point the cursor at its
 * action's target and release the ability's effect. The action-execute-start state
 * calls it on its single frame.
 *
 * A plain attack (ability 0), the knockback strike (ABILITY_ID_KNOCKBACK) and an
 * ability without an effect file play nothing. Accumulate and the Charge
 * abilities play the effect at vsync interval 1 unless it is an item effect.
 * Every other ability plays at vsync interval 2 with the tracked sound effects
 * paused; an item effect is held back until the unit shows its hold-item
 * animation. The cursor-visible flag is cleared on every path. */
void state_enter_action_execute(void) {
    unit_t* unit;
    s32 ability;

    g_game_state = GAME_STATE_ACTION_EXECUTE;
    unit = unit_get_casting();
    ability = unit->used_ability_id;
    unit->state_frame_counter = 0;
    cursor_move_to_action_target(unit);

    if ((ability == 0) || (ability == ABILITY_ID_KNOCKBACK)) {
        g_system_vsync_interval = 1;
        cursor_hide();
        return;
    }
    if (effect_ability_has_no_file(ability) != 0) {
        g_system_vsync_interval = 1;
        cursor_hide();
        return;
    }
    if ((ability == ABILITY_ID_BASIC_SKILL_ACCUMULATE)
        || (ability >= ABILITY_ID_CHARGE_FIRST && ability < ABILITY_ID_MATH_FIRST)) {
        if (effect_is_item_ability(ability) != 0) {
            g_system_vsync_interval = 1;
            cursor_hide();
            return;
        }
        effect_play();
        g_system_vsync_interval = 1;
        cursor_hide();
        return;
    }
    if ((effect_is_item_ability(ability) == 0) || ((unit->animation.sequence_id >> 1) == ANIMATION_HOLD_ITEM)) {
        effect_play();
    }
    sound_stop_background();
    g_system_vsync_interval = 2;
    cursor_hide();
}

/* Enter the resume-attack-phase state and pop the newest queued post-effect
 * message, if any, as a post-action result; the queue therefore shows in
 * reverse order. Returns 1 after showing a message and 0 when the queue is
 * empty. */
s32 action_message_show_next(void) {
    s32 index;

    g_game_state = GAME_STATE_RESUME_ATTACK_PHASE;
    g_system_vsync_interval = 1;
    unit_get_casting(); /* result unused */
    index = g_action_message_count;
    if (index != 0) {
        index -= 1;
        g_action_message_count = index;
        menu_init_system_function(MENU_SYSTEM_COMMAND_POST_ACTION_RESULT, g_action_messages[index].code,
            g_action_messages[index].unit, g_action_messages[index].value, 1);
        g_state_window_closed = 0;
        return 1;
    }
    return 0;
}

/*
 * Queue the post-action "effect message" popups for one unit whose action data
 * records a hit.
 *
 * Walks the 11 reportable outcomes of the unit's action_data_t (Speed, CT,
 * Brave, Faith, PA and MA changes, gil, an EXP loss, a level change, a broken
 * item and a stolen item) and appends one record per outcome: the
 * action_display_image_id_e of the popup, the unit's battle id and the value.
 * g_action_message_count is the record count, drained newest
 * first by action_message_show_next.
 *
 * The queue holds 16 records, but the capacity is tested once before the walk,
 * which may add up to 11 more.
 *
 * The loop over a switch is the retail shape: its 11 cases compile to the
 * jump table the binary has.
 */
static void action_message_queue_for_unit(unit_t* unit) {
    action_data_t* action;
    s32 i;
    s32 queued;
    s32 index;

    if (unit == 0) {
        return;
    }
    if (unit->stats == 0) {
        return;
    }
    if (g_action_message_count >= ACTION_POST_EFFECT_MSG_CAPACITY) {
        return;
    }
    action = &unit->stats->action;
    if (action->hit == 0) {
        return;
    }

    for (i = 0; i < 11; i++) {
        queued = 0;
        switch (i) {
        case 0:
            if (action->sp_change != 0) {
                g_action_messages[g_action_message_count].code = ACTION_DISPLAY_IMAGE_SPEED;
                g_action_messages[g_action_message_count].value = action->sp_change;
                queued = 1;
            }
            break;
        case 1:
            if (action->ct_change != 0) {
                g_action_messages[g_action_message_count].code = ACTION_DISPLAY_IMAGE_CT;
                g_action_messages[g_action_message_count].value = action->ct_change;
                queued = 1;
            }
            break;
        case 2:
            if (action->brave_change != 0) {
                g_action_messages[g_action_message_count].code = ACTION_DISPLAY_IMAGE_BRAVE;
                g_action_messages[g_action_message_count].value = action->brave_change;
                queued = 1;
            }
            break;
        case 3:
            if (action->faith_change != 0) {
                g_action_messages[g_action_message_count].code = ACTION_DISPLAY_IMAGE_FAITH_STAT;
                g_action_messages[g_action_message_count].value = action->faith_change;
                queued = 1;
            }
            break;
        case 4:
            if (action->pa_change != 0) {
                g_action_messages[g_action_message_count].code = ACTION_DISPLAY_IMAGE_PHYSICAL_ATTACK;
                g_action_messages[g_action_message_count].value = action->pa_change;
                queued = 1;
            }
            break;
        case 5:
            if (action->ma_change != 0) {
                g_action_messages[g_action_message_count].code = ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK;
                g_action_messages[g_action_message_count].value = action->ma_change;
                queued = 1;
            }
            break;
        case 6:
            if (action->gil_change > 0) {
                g_action_messages[g_action_message_count].code = ACTION_DISPLAY_IMAGE_GIL;
                g_action_messages[g_action_message_count].value = action->gil_change;
                queued = 1;
            }
            break;
        case 7:
            if (action->exp_change & ACTION_EXP_CHANGE_LOSS) {
                g_action_messages[g_action_message_count].code = ACTION_DISPLAY_IMAGE_EXP;
                g_action_messages[g_action_message_count].value = action->exp_change;
                queued = 1;
            }
            break;
        case 8:
            if (action->special_effect & (ACTION_SPECIAL_EFFECT_LEVEL_UP | ACTION_SPECIAL_EFFECT_LEVEL_DOWN)) {
                g_action_messages[g_action_message_count].code = ACTION_DISPLAY_IMAGE_LEVEL;
                g_action_messages[g_action_message_count].value = action->special_effect;
                queued = 1;
            }
            break;
        case 9:
            if (action->special_effect
                & (ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN)) {
                g_action_messages[g_action_message_count].code = ACTION_DISPLAY_IMAGE_BROKEN;
                index = g_action_message_count;
                g_action_messages[index].value = action->item_lost;
                if (action->special_effect & ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN) {
                    /* Bit 15 marks a katana that broke drawing out its power. */
                    g_action_messages[index].value = action->item_lost | 0x8000;
                }
                queued = 1;
            }
            break;
        case 10:
            if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                queued = 1;
                g_action_messages[g_action_message_count].code = ACTION_DISPLAY_IMAGE_STOLEN;
                g_action_messages[g_action_message_count].value = action->item_lost;
            }
            break;
        }
        if (queued != 0) {
            g_action_messages[g_action_message_count].unit = unit->stats->battle_id;
            g_action_message_count += 1;
        }
    }
}

/* Empty the post-effect message queue, then queue the messages of every target of
 * the casting unit. */
static void action_message_queue_for_targets(void) {
    unit_t* caster;
    s32 i;

    g_action_message_count = 0;
    caster = unit_get_casting();
    for (i = 0; i < caster->strike.target_count; i++) {
        action_message_queue_for_unit(unit_get_by_battle_id(caster->strike.target_list[i]));
    }
}

/* Queue the casting unit's own post-effect messages and show the first one. With
 * effect messages turned off, or nothing to report, an effect message with text
 * id 0 is requested instead. */
void action_message_show_for_caster(void) {
    unit_t* unit = unit_get_casting();

    action_message_queue_for_unit(unit);
    if (g_game_options.fields.effect_messages != GAME_OPTION_ON) {
        g_action_message_count = 0;
    }
    if (action_message_show_next() == 0) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
}

/* Apply the prepared action result to a unit and show it.
 *
 * The result code is kept in pending_attack_result; -1 (a rider killed on its
 * mount) finds a relocation tile and moves the unit there. The post-action
 * display and the display update follow. */
static void action_apply_unit_result(unit_t* unit) {
    unit->pending_attack_result = action_apply_attacker_result(unit->stats->battle_id);
    if (unit->pending_attack_result == -1) {
        unit_find_relocation_tile(unit->stats->battle_id, &unit->dismount);
        unit_dismount_dead_rider(unit);
    }
    result_prepare_display_by_runtime_id(unit->runtime_id);
    unit_update_display_by_runtime_id(unit->runtime_id);
}

/* Enter GAME_STATE_BATTLE_MESSAGE_DISPLAY after an action.
 *
 * Queues the targets' post-effect messages, applies the caster's own result and
 * opens the effect message window for one second. A poach or a train shows its
 * fixed message with the item-found tune. Otherwise, when some target was hit,
 * the message is the text_display entry of the ability's animation data, offset
 * by TEXT_ID_BATTLE_MESSAGE_BASE; message 6 also plays the item-found tune, and
 * messages 6 and 0x2b name the first hit target as well as the caster. Without a
 * message the window closes (text id 0). */
void state_enter_battle_message_display(void) {
    unit_t* unit;
    unit_t* target;
    animation_ability_data_t* animation;
    s32 message;
    s32 i;

    g_game_state = GAME_STATE_BATTLE_MESSAGE_DISPLAY;
    g_system_vsync_interval = 1;
    g_state_window_closed = 0;
    unit = unit_get_casting();
    action_message_queue_for_targets();
    action_apply_unit_result(unit);
    unit->item_display_active = 0;
    message_set_duration(60);
    switch (unit->strike.post_action_message) {
    case ACTION_POST_MESSAGE_POACHED:
        music_play_tune(MUSIC_TUNE_ITEM_FOUND);
        menu_init_system_function(
            MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_POACHED, unit->stats->battle_id, 0, 1);
        return;
    case ACTION_POST_MESSAGE_TRAINED:
        music_play_tune(MUSIC_TUNE_ITEM_FOUND);
        menu_init_system_function(
            MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_TRAINED, unit->stats->battle_id, 0, 1);
        return;
    }

    message = 0;
    for (i = 0; i < unit->strike.target_count; i++) {
        target = unit_get_by_battle_id(unit->strike.target_list[i]);
        if (target != 0 && target->stats != 0 && target->stats->action.hit != 0) {
            message_set_duration(60);
            /* Taking the row first keeps this loop above loop.c's size limit
             * for hoisting the constant 6 into a saved register. */
            animation = &g_ability_animation_data[unit->sp2_ability_id];
            message = animation->text_display;
            if (message == 0) {
                break;
            }
            if (message == 6) {
                music_play_tune(MUSIC_TUNE_ITEM_FOUND);
            }
            break;
        }
    }

    if (message == 6 || message == 0x2b) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, message | TEXT_ID_BATTLE_MESSAGE_BASE,
            unit->stats->battle_id, target->stats->battle_id, 1);
    } else if (message != 0) {
        menu_init_system_function(
            MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, message | TEXT_ID_BATTLE_MESSAGE_BASE, unit->stats->battle_id, 0, 1);
    } else {
        menu_init_system_function(MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
}

/* Enter the post-action report state for the source unit's current phase.
 *
 * Phase 0 reports a level up, phase 1 a job level up, and phase 2 looks for a
 * unit that learns the used ability on hit; without one the phase advances. */
void state_enter_post_action_reports(void) {
    unit_t* unit;
    unit_t* learner;
    u16 ability_id;

    g_game_state = GAME_STATE_POST_ACTION_REPORTS;
    g_system_vsync_interval = 1;
    unit = unit_get_source();
    menu_init_system_function(MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    message_set_duration(30);
    switch (g_reward_report_phase) {
    case REWARD_REPORT_LEVEL_UP:
        reward_report_level_up(unit);
        break;
    case REWARD_REPORT_JOB_LEVEL_UP:
        reward_report_job_level_up(unit);
        break;
    case REWARD_REPORT_LEARN_ON_HIT:
        g_ability_learn_on_hit_unit_id = ability_find_learn_on_hit_unit(unit->stats->battle_id, &ability_id);
        if (g_ability_learn_on_hit_unit_id != -1) {
            learner = unit_get_by_battle_id(g_ability_learn_on_hit_unit_id);
            menu_init_system_function(MENU_SYSTEM_COMMAND_ABILITY_LEARNED, ability_id, unit->stats->battle_id, 0,
                learner->stats->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
            cursor_move_to_unit(learner);
            cursor_show();
        } else {
            g_reward_report_phase++;
        }
        break;
    }
    cursor_show_hovered_unit_panel();
}

/* Start the post-action reports: restore the units' facings, apply the source
 * unit's EXP and JP rewards (clearing the display values when nothing is
 * awarded), start the earned EXP/JP display and enter the level-up phase of
 * state_enter_post_action_reports. */
void reward_grant(void) {
    unit_t* unit = unit_get_source();
    unit_stats_t* stats;

    unit_facing_restore_all_after_attack();
    if (unit != 0) {
        stats = unit->stats;
        if (stats != 0 && reward_apply_action(stats->battle_id, &unit->strike.action_rewards) == -1) {
            unit->strike.action_rewards.job_level_for_display = 0;
            unit->strike.action_rewards.level_for_display = 0;
            unit->strike.action_rewards.earned_jp = 0;
            unit->strike.action_rewards.earned_experience = 0;
        }
    }
    result_init_earned_exp_jp_display(unit);
    g_reward_report_phase = REWARD_REPORT_LEVEL_UP;
    cursor_move_to_unit(unit);
    state_enter_post_action_reports();
    g_state_window_closed = 0;
    g_state_animation_continue_check = 0;
}
