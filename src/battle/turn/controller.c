/* Drive player and AI turns from command selection through movement, targeting, action playback, rewards and scene
 * transitions. Entry operations precede frame handlers in the original program order. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/ai.h"
#include "fft/ai_decision.h"
#include "fft/animation.h"
#include "fft/at_list.h"
#include "fft/battle_map.h"
#include "fft/battle_map_load.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/crystal.h"
#include "fft/cursor.h"
#include "fft/effect.h"
#include "fft/effect_script.h"
#include "fft/effect_secondary.h"
#include "fft/fade.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/help.h"
#include "fft/input.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/message.h"
#include "fft/move.h"
#include "fft/move_step.h"
#include "fft/music.h"
#include "fft/option.h"
#include "fft/reaction.h"
#include "fft/result.h"
#include "fft/reward.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/state.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/target.h"
#include "fft/turn.h"
#include "fft/turn_command.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "fft/vector.h"
#include "fft/world_map.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

extern move_found_item_t g_move_find_display_item;

static s32 g_post_effect_message_count = 0;

static action_post_effect_msg_t g_post_effect_messages[ACTION_POST_EFFECT_MSG_CAPACITY] = { 0 };

/* Effect message texts (MENU_REQUEST_EFFECT_MESSAGE). An ability's message is the base plus
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
    unit_jumping_hide();
    script_event_state_reset();
    cursor_hide();
    g_menu_status_enabled = 0;
    g_unit_casting_runtime_id = 0xff;
    g_unit_source_runtime_id = 0xff;
}

/* Enter the event state and run the scenario check with no unit (0xff), as the
 * deployment screen does when it finishes. */
void state_enter_event_at_battle_start(void) {
    state_enter_event();
    menu_request_dispatch(MENU_REQUEST_SCENARIO_CHECK, MENU_SCENARIO_CHECK_START, 0xff, 0, 1);
}

/* Run the scenario check for the unit's turn event. A unit-ready or empty event may start a
 * scenario event (START); an ability-ready or action-result event first only tests (TEST). When
 * that test finds a pending event with a finish operation, a Ramza unit runs SKIP and any other
 * unit START (whose result is dropped); otherwise, and for every other turn event, SKIP runs.
 * Returns 1 when the check whose result is kept (for a non-Ramza unit after a successful test,
 * the test itself) dispatched an event, after saving the game state and handing the battle to
 * it. */
s32 event_try_enter_scenario(unit_runtime_t* unit) {
    s32 result;

    if (g_turn_event_type == TURN_EVENT_UNIT_READY || g_turn_event_type == TURN_EVENT_NONE) {
        result = menu_request_dispatch(
            MENU_REQUEST_SCENARIO_CHECK, MENU_SCENARIO_CHECK_START, unit->stats->battle_id, 0, 1);
    } else if (g_turn_event_type == TURN_EVENT_ABILITY_READY || g_turn_event_type == TURN_EVENT_ACTION_RESULT) {
        result = menu_request_dispatch(
            MENU_REQUEST_SCENARIO_CHECK, MENU_SCENARIO_CHECK_TEST, unit->stats->battle_id, 0, 1);
        if (result == MENU_REQUEST_RESULT_EVENT && script_event_finish_operation_get() != 0) {
            if (unit->spritesheet_id < SPRITE_SHEET_ID_RAMZA_END) {
                result = menu_request_dispatch(
                    MENU_REQUEST_SCENARIO_CHECK, MENU_SCENARIO_CHECK_SKIP, unit->stats->battle_id, 0, 1);
            } else {
                menu_request_dispatch(
                    MENU_REQUEST_SCENARIO_CHECK, MENU_SCENARIO_CHECK_START, unit->stats->battle_id, 0, 1);
            }
        } else {
            result = menu_request_dispatch(
                MENU_REQUEST_SCENARIO_CHECK, MENU_SCENARIO_CHECK_SKIP, unit->stats->battle_id, 0, 1);
        }
    } else {
        result = menu_request_dispatch(
            MENU_REQUEST_SCENARIO_CHECK, MENU_SCENARIO_CHECK_SKIP, unit->stats->battle_id, 0, 1);
    }
    if (result == MENU_REQUEST_RESULT_EVENT) {
        g_game_state_previous = g_game_state;
        state_enter_event();
        return 1;
    }
    return 0;
}

/* Copy the team flags from the unit's battle stats into its runtime record,
 * dropping UNIT_TEAM_FLAG_PLAYER_CONTROLLED unless its auto-battle setting is
 * manual. A mounted unit's partner gets the same treatment. */
void unit_store_team_flags(unit_runtime_t* unit) {
    u8 entd_flags = unit->stats->team_flags;
    u8 mount_entd_flags;
    unit_runtime_t* mount;

    unit->team_flags = entd_flags;
    if (unit->stats->auto_battle_setting != AI_AUTOBATTLE_MANUAL) {
        unit->team_flags = entd_flags & ~UNIT_TEAM_FLAG_PLAYER_CONTROLLED;
    }
    if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
        mount = unit_runtime_get(unit->mount_partner_runtime_id);
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
    unit_runtime_t* casting_unit;
    unit_runtime_t* unit;

    camera_pan_disable();
    casting_unit = unit_casting_get();
    unit = unit_source_get();
    cursor_show();
    unit_store_team_flags(casting_unit);
    if (event_try_enter_scenario(unit) == 0) {
        g_game_state = GAME_STATE_OPEN_ACTION_MENUS;
        g_state_window_closed = 0;
        camera_unit_visibility_rotate(casting_unit);
    }
}

/* Return to the free-cursor state at vsync interval 1: show the panel for the
 * cursor tile and refresh the source unit's control flags from its ENTD flags. */
void state_enter_free_cursor(void) {
    unit_runtime_t* unit;

    g_system_vsync_interval = 1;
    camera_pan_disable();
    g_game_state = GAME_STATE_FREE_CURSOR;
    g_state_in_help = 0;
    cursor_show_hovered_unit_panel();
    cursor_show();
    unit = unit_source_get();
    if (unit != 0) {
        unit_store_team_flags(unit);
    }
}

/* Select in the free cursor: enter the free-cursor help state and start building the help window. */
void state_enter_free_cursor_help(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_FREE_CURSOR_HELP;
    g_state_in_help = 1;
    help_menu_thread_start();
}

/* Reload the map when the number of crystals on the field has changed.
 *
 * Only in a Deep Dungeon battle (WORLD_MAP_RESULT_DEEP_DUNGEON_BATTLE), and not while such a reload is
 * running: the crystal count, capped at 4, is compared with script variable
 * EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, the map state that GNS rows of
 * those maps select on. A new count is stored and a map data load started; the
 * old count is put back when the load cannot start. Returns whether a reload is
 * in progress. */
static s32 deep_dungeon_crystal_count_check(void) {
    s32 crystal_count;
    s32 previous_count;

    if (g_system_frontend_world_result == WORLD_MAP_RESULT_DEEP_DUNGEON_BATTLE) {
        if (g_battle_map_deep_dungeon_load_in_progress == 0) {
            crystal_count = unit_crystals_count();
            if (crystal_count >= 5) {
                crystal_count = 4;
            }
            previous_count = script_variable_get(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT);
            if (crystal_count != previous_count) {
                script_variable_set(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, crystal_count);
                if (battle_map_load_start() != 0) {
                    g_battle_map_deep_dungeon_load_in_progress = 1;
                } else {
                    script_variable_set(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, previous_count);
                }
            }
        }
        return g_battle_map_deep_dungeon_load_in_progress;
    }
    return 0;
}

/* Between turns: when the crystal count calls for a map reload
 * (deep_dungeon_crystal_count_check), enter the Deep Dungeon
 * mesh-load state at vsync interval 1. Returns 1 when the state changed. */
static s32 deep_dungeon_mesh_load_try_start(void) {
    if (deep_dungeon_crystal_count_check() != 0) {
        g_game_state = GAME_STATE_DEEP_DUNGEON_MESH_LOAD;
        g_system_vsync_interval = 1;
        return 1;
    }

    return 0;
}

/* As deep_dungeon_mesh_load_try_start, entering the mesh-finish state
 * instead: the crystal count check made when a unit's post-movement steps end. */
static s32 deep_dungeon_mesh_finish_try_start(void) {
    if (deep_dungeon_crystal_count_check()) {
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
static void next_turn_event_take(void) {
    unit_runtime_t* unit;
    s32 action = turn_clock_run(TURN_CLOCK_MODE_RUN);
    s32 unit_id = action & 0xff;

    g_turn_event_type = action & 0xff00;
    if (g_turn_event_type == TURN_EVENT_NONE) {
        return;
    }
    unit = unit_battle_get(unit_id);
    if (unit != 0) {
        g_unit_source_runtime_id = unit->runtime_id;
    } else {
        system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    }
    switch (g_turn_event_type) {
    case TURN_EVENT_UNIT_READY:
    case TURN_EVENT_ABILITY_READY:
        unit_runtime_show(unit->runtime_id);
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
        result_runtime_display_prepare(unit->runtime_id);
        unit_runtime_display_update(unit->runtime_id);
        break;
    case TURN_EVENT_MIME:
        break;
    case 0x600: /* a label above 0x500 on the error path makes GCC split the upper
                   tree at 0x500 as the target does; nothing compares it, so the
                   original value is not recoverable */
    default:
        system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_TURN_EVENT);
        break;
    }
    unit = unit_source_get();
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
void turn_next_event_start(void) {
    unit_runtime_t* unit;

    if (deep_dungeon_mesh_load_try_start() != 0) {
        return;
    }
    at_list_reset_cursor();
    g_system_vsync_interval = 1;
    next_turn_event_take();
    unit = unit_source_get();
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
            unit_runtime_display_update(unit->runtime_id);
            sound_sfx_play(SOUND_SFX_STATUS_MENU_REQUEST);
            menu_status_open();
            at_list_show_next_entry();
            state_enter_open_action_menus();
            return;
        }
        g_state_window_closed = 0;
        if (g_turn_event_type == TURN_EVENT_UNIT_READY || g_turn_event_type == TURN_EVENT_NONE) {
            unit_runtime_display_update(unit->runtime_id);
        }
        if (event_try_enter_scenario(unit) == 0) {
            unit_view_start(UNIT_VIEW_MODE_TILE_INFO, 0xff, 0xff);
            g_game_state = GAME_STATE_CHANGE_TURN;
            ai_selected_action_init();
            g_menu_status_enabled = 1;
        } else {
            unit_runtime_display_update(unit->runtime_id);
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
    unit_runtime_t* unit;
    u32 statuses;

    camera_pan_disable();
    g_game_state = GAME_STATE_STATUS_EXECUTE;
    unit = unit_source_get();
    cursor_hide();
    g_state_window_closed = 0;
    if (g_turn_event_type == TURN_EVENT_MIME) {
        message_set_duration(60);
        menu_request_dispatch(MENU_REQUEST_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_MIME, unit->stats->battle_id,
            unit->stats->battle_id, 1);
        animation_facing_force(ANIMATION_KNEEL, unit->facing, unit);
        return;
    }
    message_set_duration(180);
    statuses = unit->statuses_to_add_5_6;
    if (statuses & UNIT_STATUS_CRYSTAL) {
        menu_request_dispatch(MENU_REQUEST_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_CRYSTAL, unit->stats->battle_id,
            unit->stats->battle_id, 1);
    } else if (statuses & UNIT_STATUS_TREASURE) {
        menu_request_dispatch(MENU_REQUEST_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_TREASURE, unit->stats->battle_id,
            unit->stats->battle_id, 1);
    } else {
        menu_request_dispatch(MENU_REQUEST_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
    unit_runtime_display_update(unit->runtime_id);
    result_runtime_display_prepare(unit->runtime_id);
}

/* Enter the AI-command state and restart the source unit's command. The
 * unit is written before the null test that guards the panel. */
static void ai_command_enter(void) {
    unit_runtime_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_AI_COMMAND;
    unit = unit_source_get();
    unit->state_frame_counter = 0;
    unit->command_ready = 0;
    target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 0);
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
    unit_runtime_t* unit;

    camera_pan_disable();
    unit = unit_source_get();
    g_unit_casting_runtime_id = unit->runtime_id;
    cursor_move_to_unit(unit);
    g_game_state = GAME_STATE_IDLING_ACTION_MENUS;

    if (unit != 0 && unit->stats != 0) {
        unit_view_start(UNIT_VIEW_MODE_UNIT_SUMMARY, unit->stats->battle_id, 0);
    }
    command_menu_open(menu_action_get_menu_id(unit->stats->battle_id));
}

/* Open the idle action menu of the unit under the cursor when it is not the
 * acting unit, or while the status menu is open; there the player and other
 * unit menus switch to their inspect variants. An empty tile does nothing. */
void menu_action_open_non_active_unit(void) {
    unit_runtime_t* unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
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
    command_menu_open(menu_id);
}

/* Open the action menu for the unit on the cursor tile, which becomes the casting
 * unit.
 *
 * The source unit, with the status menu closed, gets its own turn menu when the
 * player controls it and otherwise restarts the AI-command state. Any
 * other unit gets the non-active-unit menu. An empty tile does nothing. */
void menu_action_dispatch(void) {
    unit_runtime_t* unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);

    if (unit == 0) {
        return;
    }
    g_unit_casting_runtime_id = unit->runtime_id;
    if (g_menu_status_open == 0 && unit->runtime_id == g_unit_source_runtime_id) {
        if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
            menu_action_open_active_unit();
        } else {
            ai_command_enter();
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
    help_menu_thread_start();
}

/* Cross on a unit in the free cursor: show the casting unit's reachable tiles and enter the
 * display-move-area state with map panning, saving the cursor input mode for the Cross press that
 * ends it. */
void state_enter_display_move_area(void) {
    unit_runtime_t* unit;
    unit_stats_t* stats;
    s32 previous_mode;

    camera_pan_enable();
    g_game_state = GAME_STATE_DISPLAY_MOVE_AREA;
    unit = unit_casting_get();
    stats = unit->stats;
    move_range_build(stats->battle_id, unit->map_x, unit->map_y, unit->map_layer);
    target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_SHOW_MOVE, 1);
    previous_mode = g_cursor_input_mode;
    g_cursor_input_mode = CURSOR_INPUT_FREE_CURSOR;
    g_cursor_input_mode_saved = previous_mode;
    sound_sfx_play(SOUND_SFX_CONFIRM);
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
    target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 3);
    at_list_selection_finish();
}

/* Select in the mini menu: enter the mini-menu help state and start building the help window. */
void state_enter_mini_menu_help(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_MINI_MENU_HELP;
    g_state_in_help = 1;
    help_menu_thread_start();
}

/* Show the AT-list entry picked in the mini menu: put the cursor on its unit or, for a
 * charging action, on that action's target with the target tiles marked. */
void at_list_show_entry(s32 at_index) {
    s32 id;
    unit_runtime_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_AT_LIST_INSPECTION;
    id = at_list_get_entry_unit_id(at_index);
    if (id >= 0) {
        unit = unit_battle_get(id & AT_LIST_ENTRY_BATTLE_ID_MASK);
        if ((id & AT_LIST_ENTRY_CHARGED_ACTION) != 0) {
            cursor_move_to_action_target(unit);
            target_area_set_for_action(&unit->stats->action_command);
            target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_SHOW_TARGETS, 3);
        } else {
            cursor_move_to_unit(unit);
            target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 0);
        }
    }
    cursor_show_hovered_unit_panel();
    cursor_show();
}

/* Enter the unit-move state and show the source unit's targeting prompt. */
void state_enter_unit_move(void) {
    unit_runtime_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_UNIT_MOVE;
    unit = unit_source_get();
    menu_request_dispatch(MENU_REQUEST_TARGETING_PROMPT, MENU_TARGETING_PROMPT_DEFAULT, unit->stats->battle_id, 0,
        unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Enter the move-range-exception state, reached when the casting unit has no
 * reachable tile: opens the source unit's targeting prompt with
 * MENU_TARGETING_PROMPT_NO_MOVE_RANGE. */
static void move_range_exception_enter(void) {
    unit_runtime_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_MOVE_RANGE_EXCEPTION;
    unit = unit_source_get();
    menu_request_dispatch(MENU_REQUEST_TARGETING_PROMPT, MENU_TARGETING_PROMPT_NO_MOVE_RANGE, unit->stats->battle_id, 0,
        unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Return to choosing a move destination: recompute the source unit's reachable
 * tiles, show them and enter the select-move-tile state with the cursor unit's
 * panel. With no reachable tile the move-range exception state is entered
 * instead. The cursor-visible flag follows whether the player controls the unit. */
void state_enter_select_move_tile(void) {
    unit_runtime_t* unit;
    unit_runtime_t* casting;

    camera_pan_disable();
    unit = unit_source_get();
    casting = unit_casting_get();
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        cursor_show();
    } else {
        cursor_hide();
    }

    if (move_range_build(casting->stats->battle_id, casting->map_x, casting->map_y, casting->map_layer) > 0) {
        target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_SHOW_MOVE, 1);
        g_game_state = GAME_STATE_SELECT_MOVE_TILE;
        casting->state_frame_counter = 0;
        cursor_show_hovered_unit_panel();
    } else {
        move_range_exception_enter();
    }
}

/* Enter the illegal-move state and show move feedback that rejects the chosen
 * destination. */
void state_enter_illegal_move_menu(void) {
    unit_runtime_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_ILLEGAL_MOVE_MENU;
    unit = unit_source_get();
    menu_request_dispatch(MENU_REQUEST_MOVE_FEEDBACK, MENU_MOVE_FEEDBACK_ILLEGAL_MOVE, unit->stats->battle_id, 0,
        unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Enter the unit-moving-setup state for the chosen destination and start the source unit's move
 * feedback: confirm a pending mount or a step onto a crystal, otherwise close. A teleport (path count 0xfe or 0xff, the
 * u8 count + 2 < 2 test) asks for move confirmation instead when navigation messages are on. */
void state_enter_unit_moving_setup(void) {
    unit_runtime_t* source;
    unit_runtime_t* casting;
    unit_runtime_t* at_tile;
    u8 path_count;

    camera_pan_disable();
    g_game_state = GAME_STATE_UNIT_MOVING_SETUP;
    source = unit_source_get();
    casting = unit_casting_get();

    if (casting->path.pending_mount & MOVE_PENDING_MOUNT_SET) {
        menu_request_dispatch(MENU_REQUEST_MOVE_FEEDBACK, MENU_MOVE_FEEDBACK_MOUNT, source->stats->battle_id, 0,
            source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        return;
    }
    path_count = casting->path.count + 2;
    if (path_count < 2) {
        if (g_game_options.fields.navigation_messages != GAME_OPTION_ON) {
            menu_request_dispatch(MENU_REQUEST_MOVE_FEEDBACK, MENU_MOVE_FEEDBACK_CLOSE, source->stats->battle_id, 0,
                source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
            return;
        }
        menu_request_dispatch(MENU_REQUEST_MOVE_CONFIRM, 0, source->stats->battle_id, 0,
            source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        return;
    }
    at_tile = unit_tile_pickup_find(g_cursor_x, g_cursor_y, g_cursor_layer);
    if (at_tile != 0) {
        if (at_tile->status_flags_5_6 & UNIT_STATUS_CRYSTAL) {
            menu_request_dispatch(MENU_REQUEST_MOVE_FEEDBACK, MENU_MOVE_FEEDBACK_CRYSTAL, source->stats->battle_id, 0,
                source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
            return;
        }
    }
    menu_request_dispatch(MENU_REQUEST_MOVE_FEEDBACK, MENU_MOVE_FEEDBACK_CLOSE, source->stats->battle_id, 0,
        source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Save the unit's tile, facing, mount link and stood-on unit before it moves;
 * previous_state_restore restores them. Every field is read
 * before any is stored, as the retail code orders them. */
static void previous_state_save(unit_runtime_t* unit) {
    u8 x = unit->map_x;
    u8 y = unit->map_y;
    u8 layer = unit->map_layer;
    u16 facing = unit->facing;
    u8 mount_value = unit->mount_state;
    u8 mount_id = unit->mount_partner_runtime_id;
    u8 standing_on_battle_id_plus_one = unit->standing_on_battle_id_plus_one;

    unit->previous_map_x = x;
    unit->previous_map_y = y;
    unit->previous_map_layer = layer;
    unit->previous_facing = facing;
    unit->previous_mount_state = mount_value;
    unit->previous_mount_partner_runtime_id = mount_id;
    unit->previous_standing_on_battle_id_plus_one = standing_on_battle_id_plus_one;
}

/* Enter the unit-moving state: save the casting unit's tile, facing and mount
 * state for an undo (and its partner's when it is a mount) and start its path
 * from the first step at speed 0x2000. */
void state_enter_unit_moving(void) {
    unit_runtime_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_UNIT_MOVING;
    cursor_show_hovered_unit_panel();
    unit = unit_casting_get();
    previous_state_save(unit);
    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        unit_runtime_t* mount = unit_runtime_get(unit->mount_partner_runtime_id);
        if (mount != 0) {
            previous_state_save(mount);
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
    unit_runtime_t* source;
    unit_runtime_t* caster;

    camera_pan_disable();
    g_game_state = GAME_STATE_MOVE_CONFIRM_MENU;
    source = unit_source_get();
    caster = unit_casting_get();
    message_set_duration(60);
    if (caster->path.flags & MOVE_EFFECTIVE_TELEPORT) {
        if (caster->last_path_count == MOVE_PATH_TELEPORT_FAILED) {
            menu_request_dispatch(
                MENU_REQUEST_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_TELEPORT_FAILED, source->stats->battle_id, 0, 1);
        } else {
            menu_request_dispatch(MENU_REQUEST_MOVE_CONFIRM, 0, source->stats->battle_id, 0, 0);
        }
        caster->last_path_count = 0;
    } else {
        if (g_game_options.fields.navigation_messages != GAME_OPTION_ON) {
            sprite_palette_tint_by_team(source, 2);
            menu_request_dispatch(MENU_REQUEST_MOVE_CONFIRM_SILENT, 0, source->stats->battle_id, 0,
                source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            sprite_palette_tint_by_team(source, 2);
            menu_request_dispatch(MENU_REQUEST_MOVE_CONFIRM, 0, source->stats->battle_id, 0,
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
    cursor_move_to_unit(unit_source_get());
    menu_request_dispatch(MENU_REQUEST_CLOSE, 0, 0, 0, 0);
}

/* Turn a unit to one of the four cardinal facings and restart its animation.
 * A diagonal or any other angle leaves it as it is. */
void unit_facing_set_cardinal(unit_runtime_t* unit, s32 facing) {
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
    animation_status_set(unit);
}

/* Report a level up recorded in the unit's rewards: its message for three
 * seconds, the level-up animation and effect, the cursor on the unit and the
 * level-up tune. Returns 1 after reporting one, otherwise 0. */
static s32 level_up_report(unit_runtime_t* unit) {
    if (unit->strike.action_rewards.level_for_display != 0) {
        u8 battle_id;

        message_set_duration(180);
        battle_id = unit->stats->battle_id;
        menu_request_dispatch(MENU_REQUEST_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_LEVEL_UP, battle_id, battle_id, 1);
        animation_level_up_set(unit);
        cursor_move_to_unit(unit);
        effect_secondary_level_up_start(unit);
        music_tune_play(MUSIC_TUNE_LEVEL_UP);
        return 1;
    }
    return 0;
}

/* Report a job level up recorded in the unit's rewards: its message for three
 * seconds, the level-up animation and effect, the cursor on the unit and the job
 * level-up tune. Returns 1 after reporting one, otherwise 0. */
static s32 job_level_up_report(unit_runtime_t* unit) {
    if (unit->strike.action_rewards.job_level_for_display != 0) {
        u8 battle_id;

        message_set_duration(180);
        battle_id = unit->stats->battle_id;
        menu_request_dispatch(
            MENU_REQUEST_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_JOB_LEVEL_UP, battle_id, battle_id, 1);
        animation_level_up_set(unit);
        cursor_move_to_unit(unit);
        effect_secondary_level_up_start(unit);
        music_tune_play(MUSIC_TUNE_JOB_LEVEL_UP);
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
    unit_runtime_t* source = unit_source_get();
    unit_runtime_t* casting = unit_casting_get();
    unit_runtime_t* target;
    unit_stats_t* stats;
    s32 item;
    s32 animation;
    s32 trap;

    menu_request_dispatch(MENU_REQUEST_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
    g_game_state = GAME_STATE_POST_MOVE_EVENTS;
    g_state_window_closed = 0;
    casting->state_frame_counter = 0;
    if (g_move_post_events & MOVE_POST_EVENT_CRYSTAL_OR_TREASURE) {
        target = unit_tile_pickup_find(source->map_x, source->map_y, source->map_layer);
        /* -1 for a crystal, otherwise the treasure's item id. */
        item = crystal_resolve_pickup(source->stats->battle_id);
        if (item == -1) {
            animation = ANIMATION_RAISE_HANDS;
        } else {
            sprite_display_store_item(source, item);
            animation = ANIMATION_HOLD_ITEM;
        }
        animation_facing_force(animation, source->facing, source);
        if (item == -1) {
            /* A player-controlled unit of the player team picks from the
             * crystal menu; any other unit takes the HP/MP restore at once. */
            stats = source->stats;
            if (!(stats->initial_team_flags & UNIT_TEAM_MASK)
                && (stats->initial_team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED)) {
                menu_request_dispatch(MENU_REQUEST_CRYSTAL_OR_TREASURE, stats->battle_id, stats->battle_id, 0, 1);
            } else {
                menu_request_dispatch(MENU_REQUEST_CRYSTAL_OR_TREASURE, stats->battle_id, stats->battle_id, 0, 0);
                crystal_choice_apply(source->stats, CRYSTAL_RESTORE_HP_MP);
                action_apply_target_result(source->stats->battle_id);
                result_display_prepare(source);
                unit_runtime_display_update(source->runtime_id);
            }
        } else {
            if (!(source->stats->initial_team_flags & UNIT_TEAM_MASK)) {
                menu_request_dispatch(
                    MENU_REQUEST_CRYSTAL_OR_TREASURE, source->stats->battle_id, source->stats->battle_id, 0, 1);
            } else {
                menu_request_dispatch(
                    MENU_REQUEST_CRYSTAL_OR_TREASURE, source->stats->battle_id, source->stats->battle_id, 0, 0);
            }
            if ((source->stats->initial_team_flags & UNIT_TEAM_MASK)
                || !(source->stats->initial_team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED)) {
                crystal_choice_apply(source->stats, CRYSTAL_TAKE_TREASURE);
            }
        }
        unit_fade_out_start_red(target->runtime_id);
        target->depth_height_offset
            = 0x24 - (unit_height_calculate_from_world_coords(source) - target->world_coords.vy);
        sound_free_channel_sfx_play(SOUND_SFX_COMPLETE);
        return 1;
    }
    if (g_move_post_events & MOVE_POST_EVENT_MOVEMENT_BENEFIT) {
        switch (g_reward_report_phase) {
        case REWARD_REPORT_LEVEL_UP:
            move_init_ability_benefit(casting->stats);
            reward_apply_result_and_collect(casting->stats, &casting->strike.action_rewards);
            result_display_prepare(casting);
            result_exp_jp_display_init(casting);
            level_up_report(casting);
            return 1;
        case REWARD_REPORT_JOB_LEVEL_UP:
            job_level_up_report(casting);
            return 1;
        }
        return 1;
    }
    if (g_move_post_events & MOVE_POST_EVENT_ITEM_FOUND) {
        g_move_find_display_item = *move_find_item_determine(casting->stats);
        sprite_display_store_item(source, g_move_find_display_item.item_id);
        animation_facing_force(ANIMATION_FOUND_ITEM, source->facing, source);
        if (!(source->team_flags & UNIT_TEAM_MASK)) {
            menu_request_dispatch(
                MENU_REQUEST_MOVE_FIND_ITEM, g_move_find_display_item.item_id, source->stats->battle_id, 0, 1);
            music_tune_play(MUSIC_TUNE_ITEM_FOUND);
            return 1;
        }
        menu_request_dispatch(
            MENU_REQUEST_MOVE_FIND_ITEM, g_move_find_display_item.item_id, source->stats->battle_id, 0, 0);
        return 1;
    }
    if (g_move_post_events & MOVE_POST_EVENT_TRAP_TRIGGERED) {
        trap = move_find_prepare_trap_result(casting->stats);
        if ((source->pending_attack_result = action_apply_target_result(source->stats->battle_id)) == -1) {
            unit_find_relocation_tile(source->stats->battle_id, &source->dismount);
            unit_dismount_dead_rider(source);
        }
        result_display_prepare(source);
        action_start_target_response(0, source);
        effect_secondary_trap_start(source);
        menu_request_dispatch(MENU_REQUEST_TRAP_RESULT, trap, source->stats->battle_id, 0, 1);
        ai_triggered_trap_coords_store(casting->map_x, casting->map_layer, casting->map_y);
        return 1;
    }
    if (g_move_post_events & MOVE_POST_EVENT_CHARGING_CANCEL) {
        action_charge_check_and_cancel(casting->stats, 1);
        menu_request_dispatch(MENU_REQUEST_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
        return 1;
    }
    if (g_move_post_events & MOVE_POST_EVENT_SOURCE_DISPLAY) {
        menu_request_dispatch(MENU_REQUEST_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
        result_runtime_display_prepare(source->runtime_id);
        unit_runtime_display_update(source->runtime_id);
        return 1;
    }
    if (g_move_post_events & MOVE_POST_EVENT_MOUNT_STATUS_CHANGE) {
        menu_request_dispatch(MENU_REQUEST_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
        if (source->mount_state != UNIT_MOUNT_STATE_NONE) {
            target = unit_runtime_get(source->mount_partner_runtime_id);
            if (target != 0) {
                result_runtime_display_prepare(target->runtime_id);
                unit_runtime_display_update(target->runtime_id);
            }
        }
        return 1;
    }
    unit_runtime_display_update(source->runtime_id);
    unit_runtime_display_update(casting->runtime_id);
    if (deep_dungeon_mesh_finish_try_start() == 0) {
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
    stats = unit_casting_get()->stats;
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
    unit_runtime_t* mounted_unit;
    unit_runtime_t* unit;
    s32 facing;

    camera_pan_disable();
    unit = unit_source_get();
    facing = unit->facing;
    g_game_state = GAME_STATE_WAIT_DIRECTION;
    g_state_wait_direction_facing = facing;
    unit->state_frame_counter = 0;
    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        mounted_unit = unit_runtime_get(unit->mount_partner_runtime_id);
        if (mounted_unit != 0) {
            unit_facing_set_cardinal(unit, mounted_unit->facing);
            if (unit->stats != 0) {
                unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_layer,
                    (mounted_unit->facing / UNIT_FACING_QUARTER_TURN));
            }
        }
    }
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) == 0) {
        unit->command_state.cursor.facing_hint = ai_wait_facing_choose();
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
    unit_runtime_t* unit;

    camera_pan_disable();
    unit = unit_source_get();
    if (menu_request_dispatch(MENU_REQUEST_SCENARIO_CHECK, MENU_SCENARIO_CHECK_TEST, unit->stats->battle_id, 0, 1)
            == MENU_REQUEST_RESULT_EVENT
        && script_event_finish_operation_get() != 0) {
        g_game_state_previous = g_game_state;
        menu_request_dispatch(MENU_REQUEST_SCENARIO_CHECK, MENU_SCENARIO_CHECK_START, unit->stats->battle_id, 0, 1);
        state_enter_event();
        return;
    }
    if (turn_status_flags_calculate(unit->stats) & TURN_STATUS_BLOCKS_WAIT_MENU_MASK) {
        turn_next_event_start();
    } else {
        g_game_state = GAME_STATE_WAIT_MENU;
        cursor_show_hovered_unit_panel();
        menu_request_dispatch(
            MENU_REQUEST_WAIT_MENU, 0, unit->stats->battle_id, 0, unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
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
void unit_facing_turn_to_cursor(unit_runtime_t* unit) {
    VECTOR difference;
    s32 unit_x;
    s32 unit_y;

    if (unit != 0) {
        s32 cursor_x = g_cursor_x;
        s32 cursor_y = g_cursor_y;
        s32 tile_x;
        s32 tile_y;
        /* Retail retains these zero stores before overwriting both components. */
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
 * attack_facing so facing_after_attack_restore can turn it back
 * once the action is over. The retail code reads attack_facing with a signed
 * `lh`, hence the s16 access. */
static void facing_before_attack_save(unit_runtime_t* unit) {
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
void unit_facing_turn_to_target(unit_runtime_t* unit, unit_runtime_t* target) {
    VECTOR delta;
    unit_runtime_t* other;

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
        other = unit_battle_get(unit->stats->action_command.target_id);
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
    facing_before_attack_save(unit);
    unit->facing = (ratan2(-delta.vz, delta.vx) - 0x200) & 0xc00;
    unit_set_tile_position(
        unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_layer, unit->facing / UNIT_FACING_QUARTER_TURN);
    if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
        unit_runtime_get(unit->mount_partner_runtime_id)->facing = unit->facing;
        unit_set_tile_position(
            unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_layer, unit->facing / UNIT_FACING_QUARTER_TURN);
    }
}

/* Turns a living unit back to the facing saved in attack_facing (-1 for none) and
 * clears the saved value; a dead unit only has it cleared.
 *
 * The battle stats take the facing in quarter turns through
 * unit_set_tile_position. For a mounted pair the partner record's facing
 * is copied too, without a null test, and the tile position is stored a second
 * time under the unit's own id. */
static void facing_after_attack_restore(unit_runtime_t* unit) {
    unit_runtime_t* mount;
    s16 facing;
    s32 unused[4]; /* the retail stack frame has 16 unused bytes */

    if (unit != 0) {
        if ((unit->status_flags_1_4 & UNIT_STATUS_DEAD) == 0) {
            /* The facing 16-bit values are read signed (lh) here. */
            facing = unit->attack_facing;
            if (facing != -1) {
                unit->facing = facing;
                unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_layer,
                    facing / UNIT_FACING_QUARTER_TURN);
                /* Stored as a signed 16-bit value (li -1, not ori 0xffff). */
                unit->attack_facing = -1;
                if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
                    mount = unit_runtime_get(unit->mount_partner_runtime_id);
                    mount->facing = unit->facing;
                    unit_set_tile_position(unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_layer,
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
void unit_facing_restore_targets_after_attack(unit_runtime_t* unit) {
    s32 i;
    unit_runtime_t* target;

    if (unit->strike.target_count != 0) {
        for (i = 0; i < unit->strike.target_count; i++) {
            target = unit_battle_get(unit->strike.target_list[i]);
            if (target != 0) {
                facing_after_attack_restore(target);
            }
        }
    }
}

/* facing_after_attack_restore for every runtime id 0..15. */
static void facing_all_after_attack_restore(void) {
    s32 i;

    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        unit_runtime_t* unit = unit_runtime_get(i);

        if (unit != 0) {
            facing_after_attack_restore(unit);
        }
    }
}

/* Enter the continue-turn state at vsync interval 1 and restart the source unit's
 * state frame counter. */
void state_enter_continue_turn(void) {
    unit_runtime_t* unit;

    g_system_vsync_interval = 1;
    g_game_state = GAME_STATE_CONTINUE_TURN;
    unit = unit_source_get();
    if (unit != 0) {
        unit->state_frame_counter = 0;
    }
}

/* Ends a unit's command: the cursor returns to the unit, its display is updated
 * and the game enters the after-command state. */
void action_end_command(unit_runtime_t* unit) {
    cursor_move_to_unit(unit);
    unit_runtime_display_update(unit->runtime_id);
    state_enter_after_command();
}

/*
 * Enter the targeting-prompt state after the ability preview: the
 * targeting prompt with MENU_TARGETING_PROMPT_NO_TARGET when the preview marked
 * no tile (TARGET_PANELS_NONE), MENU_TARGETING_PROMPT_DEFAULT otherwise. The cursor-visible flag then follows whether
 * the player controls the unit.
 */
void state_enter_targeting_prompt(void) {
    unit_runtime_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_TARGETING_PROMPT;
    unit = unit_source_get();
    if (unit->ability_preview_phase == TARGET_PANELS_NONE)
        menu_request_dispatch(MENU_REQUEST_TARGETING_PROMPT, MENU_TARGETING_PROMPT_NO_TARGET, unit->stats->battle_id, 0,
            unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
    else
        menu_request_dispatch(MENU_REQUEST_TARGETING_PROMPT, MENU_TARGETING_PROMPT_DEFAULT, unit->stats->battle_id, 0,
            unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
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
    unit_runtime_t* unit;
    s32 ability;

    g_game_state = GAME_STATE_ACTION_EXECUTE;
    unit = unit_casting_get();
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
    sound_background_stop();
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
    unit_casting_get(); /* result unused */
    index = g_post_effect_message_count;
    if (index != 0) {
        index -= 1;
        g_post_effect_message_count = index;
        menu_request_dispatch(MENU_REQUEST_POST_ACTION_RESULT, g_post_effect_messages[index].code,
            g_post_effect_messages[index].unit, g_post_effect_messages[index].value, 1);
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
 * g_post_effect_message_count is the record count, drained newest
 * first by action_message_show_next.
 *
 * The queue holds 16 records, but the capacity is tested once before the walk,
 * which may add up to 11 more.
 *
 * The loop over a switch is the retail shape: its 11 cases compile to the
 * jump table the binary has.
 */
static void message_for_unit_queue(unit_runtime_t* unit) {
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
    if (g_post_effect_message_count >= ACTION_POST_EFFECT_MSG_CAPACITY) {
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
                g_post_effect_messages[g_post_effect_message_count].code = ACTION_DISPLAY_IMAGE_SPEED;
                g_post_effect_messages[g_post_effect_message_count].value = action->sp_change;
                queued = 1;
            }
            break;
        case 1:
            if (action->ct_change != 0) {
                g_post_effect_messages[g_post_effect_message_count].code = ACTION_DISPLAY_IMAGE_CT;
                g_post_effect_messages[g_post_effect_message_count].value = action->ct_change;
                queued = 1;
            }
            break;
        case 2:
            if (action->brave_change != 0) {
                g_post_effect_messages[g_post_effect_message_count].code = ACTION_DISPLAY_IMAGE_BRAVE;
                g_post_effect_messages[g_post_effect_message_count].value = action->brave_change;
                queued = 1;
            }
            break;
        case 3:
            if (action->faith_change != 0) {
                g_post_effect_messages[g_post_effect_message_count].code = ACTION_DISPLAY_IMAGE_FAITH_STAT;
                g_post_effect_messages[g_post_effect_message_count].value = action->faith_change;
                queued = 1;
            }
            break;
        case 4:
            if (action->pa_change != 0) {
                g_post_effect_messages[g_post_effect_message_count].code = ACTION_DISPLAY_IMAGE_PHYSICAL_ATTACK;
                g_post_effect_messages[g_post_effect_message_count].value = action->pa_change;
                queued = 1;
            }
            break;
        case 5:
            if (action->ma_change != 0) {
                g_post_effect_messages[g_post_effect_message_count].code = ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK;
                g_post_effect_messages[g_post_effect_message_count].value = action->ma_change;
                queued = 1;
            }
            break;
        case 6:
            if (action->gil_change > 0) {
                g_post_effect_messages[g_post_effect_message_count].code = ACTION_DISPLAY_IMAGE_GIL;
                g_post_effect_messages[g_post_effect_message_count].value = action->gil_change;
                queued = 1;
            }
            break;
        case 7:
            if (action->exp_change & ACTION_EXP_CHANGE_LOSS) {
                g_post_effect_messages[g_post_effect_message_count].code = ACTION_DISPLAY_IMAGE_EXP;
                g_post_effect_messages[g_post_effect_message_count].value = action->exp_change;
                queued = 1;
            }
            break;
        case 8:
            if (action->special_effect & (ACTION_SPECIAL_EFFECT_LEVEL_UP | ACTION_SPECIAL_EFFECT_LEVEL_DOWN)) {
                g_post_effect_messages[g_post_effect_message_count].code = ACTION_DISPLAY_IMAGE_LEVEL;
                g_post_effect_messages[g_post_effect_message_count].value = action->special_effect;
                queued = 1;
            }
            break;
        case 9:
            if (action->special_effect
                & (ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN)) {
                g_post_effect_messages[g_post_effect_message_count].code = ACTION_DISPLAY_IMAGE_BROKEN;
                index = g_post_effect_message_count;
                g_post_effect_messages[index].value = action->item_lost;
                if (action->special_effect & ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN) {
                    /* Bit 15 marks a katana that broke drawing out its power. */
                    g_post_effect_messages[index].value = action->item_lost | 0x8000;
                }
                queued = 1;
            }
            break;
        case 10:
            if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                queued = 1;
                g_post_effect_messages[g_post_effect_message_count].code = ACTION_DISPLAY_IMAGE_STOLEN;
                g_post_effect_messages[g_post_effect_message_count].value = action->item_lost;
            }
            break;
        }
        if (queued != 0) {
            g_post_effect_messages[g_post_effect_message_count].unit = unit->stats->battle_id;
            g_post_effect_message_count += 1;
        }
    }
}

/* Empty the post-effect message queue, then queue the messages of every target of
 * the casting unit. */
static void message_for_targets_queue(void) {
    unit_runtime_t* caster;
    s32 i;

    g_post_effect_message_count = 0;
    caster = unit_casting_get();
    for (i = 0; i < caster->strike.target_count; i++) {
        message_for_unit_queue(unit_battle_get(caster->strike.target_list[i]));
    }
}

/* Queue the casting unit's own post-effect messages and show the first one. With
 * effect messages turned off, or nothing to report, an effect message with text
 * id 0 is requested instead. */
void action_message_show_for_caster(void) {
    unit_runtime_t* unit = unit_casting_get();

    message_for_unit_queue(unit);
    if (g_game_options.fields.effect_messages != GAME_OPTION_ON) {
        g_post_effect_message_count = 0;
    }
    if (action_message_show_next() == 0) {
        menu_request_dispatch(MENU_REQUEST_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
}

/* Apply the prepared action result to a unit and show it.
 *
 * The result code is kept in pending_attack_result; -1 (a rider killed on its
 * mount) finds a relocation tile and moves the unit there. The post-action
 * display and the display update follow. */
static void unit_result_apply(unit_runtime_t* unit) {
    unit->pending_attack_result = action_apply_attacker_result(unit->stats->battle_id);
    if (unit->pending_attack_result == -1) {
        unit_find_relocation_tile(unit->stats->battle_id, &unit->dismount);
        unit_dismount_dead_rider(unit);
    }
    result_runtime_display_prepare(unit->runtime_id);
    unit_runtime_display_update(unit->runtime_id);
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
    unit_runtime_t* unit;
    unit_runtime_t* target;
    animation_ability_data_t* animation;
    s32 message;
    s32 i;

    g_game_state = GAME_STATE_BATTLE_MESSAGE_DISPLAY;
    g_system_vsync_interval = 1;
    g_state_window_closed = 0;
    unit = unit_casting_get();
    message_for_targets_queue();
    unit_result_apply(unit);
    unit->item_display_active = 0;
    message_set_duration(60);
    switch (unit->strike.post_action_message) {
    case ACTION_POST_MESSAGE_POACHED:
        music_tune_play(MUSIC_TUNE_ITEM_FOUND);
        menu_request_dispatch(
            MENU_REQUEST_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_POACHED, unit->stats->battle_id, 0, 1);
        return;
    case ACTION_POST_MESSAGE_TRAINED:
        music_tune_play(MUSIC_TUNE_ITEM_FOUND);
        menu_request_dispatch(
            MENU_REQUEST_EFFECT_MESSAGE, TEXT_ID_BATTLE_MESSAGE_TRAINED, unit->stats->battle_id, 0, 1);
        return;
    }

    message = 0;
    for (i = 0; i < unit->strike.target_count; i++) {
        target = unit_battle_get(unit->strike.target_list[i]);
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
                music_tune_play(MUSIC_TUNE_ITEM_FOUND);
            }
            break;
        }
    }

    if (message == 6 || message == 0x2b) {
        menu_request_dispatch(MENU_REQUEST_EFFECT_MESSAGE, message | TEXT_ID_BATTLE_MESSAGE_BASE,
            unit->stats->battle_id, target->stats->battle_id, 1);
    } else if (message != 0) {
        menu_request_dispatch(
            MENU_REQUEST_EFFECT_MESSAGE, message | TEXT_ID_BATTLE_MESSAGE_BASE, unit->stats->battle_id, 0, 1);
    } else {
        menu_request_dispatch(MENU_REQUEST_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
}

/* Enter the post-action report state for the source unit's current phase.
 *
 * Phase 0 reports a level up, phase 1 a job level up, and phase 2 looks for a
 * unit that learns the used ability on hit; without one the phase advances. */
void state_enter_post_action_reports(void) {
    unit_runtime_t* unit;
    unit_runtime_t* learner;
    u16 ability_id;

    g_game_state = GAME_STATE_POST_ACTION_REPORTS;
    g_system_vsync_interval = 1;
    unit = unit_source_get();
    menu_request_dispatch(MENU_REQUEST_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    message_set_duration(30);
    switch (g_reward_report_phase) {
    case REWARD_REPORT_LEVEL_UP:
        level_up_report(unit);
        break;
    case REWARD_REPORT_JOB_LEVEL_UP:
        job_level_up_report(unit);
        break;
    case REWARD_REPORT_LEARN_ON_HIT:
        g_ability_learn_on_hit_unit_id = ability_find_learn_on_hit_unit(unit->stats->battle_id, &ability_id);
        if (g_ability_learn_on_hit_unit_id != -1) {
            learner = unit_battle_get(g_ability_learn_on_hit_unit_id);
            menu_request_dispatch(MENU_REQUEST_ABILITY_LEARNED, ability_id, unit->stats->battle_id, 0,
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
    unit_runtime_t* unit = unit_source_get();
    unit_stats_t* stats;

    facing_all_after_attack_restore();
    if (unit != 0) {
        stats = unit->stats;
        if (stats != 0 && reward_apply_action(stats->battle_id, &unit->strike.action_rewards) == -1) {
            unit->strike.action_rewards.job_level_for_display = 0;
            unit->strike.action_rewards.level_for_display = 0;
            unit->strike.action_rewards.earned_jp = 0;
            unit->strike.action_rewards.earned_experience = 0;
        }
    }
    result_exp_jp_display_init(unit);
    g_reward_report_phase = REWARD_REPORT_LEVEL_UP;
    cursor_move_to_unit(unit);
    state_enter_post_action_reports();
    g_state_window_closed = 0;
    g_state_animation_continue_check = 0;
}

/* Build the ability-effect context and hand it to the effect selector.
 *
 * Each of the acting unit's targets contributes a target block carrying its
 * runtime id and a result animation. With no targets and empty-tile effects
 * enabled, the first block holds the post-action destination tile. Every other
 * unit with stats that is neither the caster nor a target is appended to
 * spectator_ids. The packed
 * argument combines the ability's charge animation set with the element used
 * as the effect's sprite palette, and stays 0 while the action has no attack
 * to animate. The failed-condition path is written as its own `packed = 0`
 * block: the target keeps that block, and a pre-initialized `packed` lets GCC
 * thread the failing tests straight to the call. Items 0x4a..0x4c (the
 * elemental guns) and ability 0x189 (Ninja Ball) are hard-coded exceptions. */
static void action_effect_start(s32 ability, unit_runtime_t* unit) {
    effect_context_t init;
    unit_runtime_t* target;
    unit_runtime_t* spectator;
    unit_stats_t* target_stats;
    unit_stats_t* spectator_stats;
    s32 i;
    s32 j;
    s32 spectators;
    u32 packed;
    u8 item;
    action_strike_work_t* block;

    init.target_count = unit->strike.target_count;
    init.targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    init.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = unit->runtime_id;
    vector_short_set(
        &init.target_tile, unit->strike.target_tile.x, unit->strike.target_tile.layer, unit->strike.target_tile.y);
    block = &unit->strike;
    if (init.target_count != 0) {
        for (i = 0; i < init.target_count; i++) {
            target = unit_battle_get(block->target_list[i]);
            init.targets[i].target_type = EFFECT_TARGET_TYPE_UNIT;
            init.targets[i].data.unit.runtime_id = target->runtime_id;
            target_stats = target->stats;
            if (target_stats == 0) {
                init.targets[i].data.unit.result_animation = EFFECT_HIT_NORMAL;
            } else if (target_stats->action.critical != 0) {
                init.targets[i].data.unit.result_animation = EFFECT_HIT_CRITICAL;
            } else if (target_stats->action.miss_type == ACTION_MISS_TYPE_REFLECTED) {
                init.targets[i].data.unit.result_animation = EFFECT_HIT_REFLECTED;
            } else if (target_stats->action.miss_type != ACTION_MISS_TYPE_HIT) {
                init.targets[i].data.unit.result_animation = EFFECT_HIT_MISSED;
            } else {
                init.targets[i].data.unit.result_animation = EFFECT_HIT_NORMAL;
            }
        }
    } else if (unit->strike.skip_empty_tile_effect == 0) {
        init.target_count = 1;
        init.targets[0].target_type = EFFECT_TARGET_TYPE_TILE;
        init.targets[0].data.tile.vx = unit->strike.target_tile.x;
        init.targets[0].data.tile.vz = unit->strike.target_tile.y;
        init.targets[0].data.tile.vy = unit->strike.target_tile.layer;
    }
    spectators = 0;
    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        if (i != unit->runtime_id) {
            spectator = unit_runtime_get(i);
            if (spectator != 0) {
                spectator_stats = spectator->stats;
                if (spectator_stats != 0) {
                    for (j = 0; j < block->target_count; j++) {
                        if (spectator_stats->battle_id == block->target_list[j]) {
                            break;
                        }
                    }
                    if (j < block->target_count) {
                        continue;
                    }
                    init.spectator_ids[spectators++] = spectator->runtime_id;
                }
            }
        }
    }
    init.spectator_count = spectators;
    init.math_skill_flag = block->math_skill_flag;
    init.used_weapon_id = block->used_weapon_id;

    /* The subtraction tests for Blaze Gun or Glacier Gun; the three elemental guns pass no
     * charge set or element. */
    if (block->last_attack_id == 0 || block->reaction_occurred != 0 || g_action_phase != ACTION_PHASE_ACTING_UNIT
        || (unit->used_item_or_weapon_id - ITEM_ID_BLAZE_GUN) < 2U
        || (item = unit->used_item_or_weapon_id) == ITEM_ID_BLAST_GUN) {
        packed = 0;
    } else {
        if (ability == ABILITY_ID_THROW_BALL) {
            packed = g_ability_animation_data[ABILITY_ID_THROW_BALL].charge_animation_set_id;
            packed |= g_item_weapon_data[item].element << 16;
        } else {
            packed = g_ability_animation_data[ability].charge_animation_set_id;
            packed |= g_ability_range_data[ability].element << 16;
        }
    }
    effect_ability_animation_set(packed, ability, &init);
}

/* Announce the next action that comes due: a Hamedo, the acting unit's own action or a reaction.
 *
 * Each of the three action phases is polled in turn; a phase that produces a
 * unit id publishes the announcement state, resolves the spell quote and stops
 * the scan.  Returns 1 once an announcement was set up, 0 when all three phases
 * had nothing pending. */
s32 action_announce_next(void) {
    action_command_t command;
    unit_runtime_t* unit = unit_source_get();
    unit_runtime_t* actor;
    s32 result;
    s32 ability_id;
    unit_stats_t* actor_stats;

    do {
        switch (g_action_phase) {
        case ACTION_PHASE_PRE_ACTION:
            result = reaction_prepare_hamedo(unit->stats->battle_id, &command.ability_id);
            if (result != -1) {
                g_game_state = GAME_STATE_ACTION_ANNOUNCEMENT;
                actor = unit_battle_get(result);
                g_unit_casting_runtime_id = actor->runtime_id;
                command.skillset_id = 0;
                actor_stats = actor->stats;
                announcement_select(&command, actor_stats->battle_id, 1);
                ability_id = command.ability_id;
                result = 0;
                actor->continue_attack_count = 0;
                g_state_window_closed = 0;
                actor->sp2_ability_id = ability_id;
                actor->used_ability_id = ability_id;
            }
            break;
        case ACTION_PHASE_ACTING_UNIT:
            result = action_init_current_data(unit->stats->battle_id);
            if (result == 0) {
                g_game_state = GAME_STATE_ACTION_ANNOUNCEMENT;
                g_unit_casting_runtime_id = unit->runtime_id;
                announcement_select(&unit->stats->action_command, unit->stats->battle_id, 1);
                ability_id = unit->command_state.command.data.action.ability_id;
                unit->continue_attack_count = 0;
                g_state_window_closed = 0;
                unit->sp2_ability_id = ability_id;
                unit->used_ability_id = ability_id;
            } else {
                if (result >= 0) {
                    if (result < 4) {
                        result_charge_failure_queue(unit, result);
                    }
                }
                animation_status_set(unit);
                action_end_command(unit);
                result = -1;
            }
            break;
        case ACTION_PHASE_REACTING_UNIT:
            result = reaction_prepare_next(&command.ability_id);
            if (result != -1) {
                g_game_state = GAME_STATE_ACTION_ANNOUNCEMENT;
                actor = unit_battle_get(result);
                g_unit_casting_runtime_id = actor->runtime_id;
                command.skillset_id = 0;
                actor_stats = actor->stats;
                announcement_select(&command, actor_stats->battle_id, 1);
                ability_id = command.ability_id;
                result = 0;
                actor->continue_attack_count = 0;
                g_state_window_closed = 0;
                actor->sp2_ability_id = ability_id;
                actor->used_ability_id = ability_id;
            }
            break;
        }
        if (result != -1) {
            return 1;
        }
        g_action_phase = g_action_phase + 1;
    } while (g_action_phase < ACTION_PHASE_COUNT);
    return 0;
}

/* Blaze Gun, Glacier Gun and Blast Gun (item ids 0x4a..0x4c) play the weapon
 * strike even when an ability is used. */
#define IS_ELEMENTAL_GUN(id) ((id == ITEM_ID_BLAZE_GUN || id == ITEM_ID_GLACIER_GUN) || id == ITEM_ID_BLAST_GUN)

/* Enters GAME_STATE_ACTION_EXECUTE_START, finalizes the casting unit's action and picks its attack
 * animation.
 *
 * Each target's attack result is resolved first; for the knockback strike
 * (ABILITY_ID_KNOCKBACK) in a later phase the targets are moved to the
 * knockback destination instead.
 * An ability picks the ability animation unless a reaction occurred, while a
 * plain attack or an elemental gun picks the weapon strike.
 *
 * continue_attack_count is nonzero on the follow-up strikes of a continued
 * attack. */
void action_apply_strike_results(void) {
    unit_runtime_t* unit;
    unit_runtime_t* target;
    action_strike_work_t* action;
    s32 i;

    g_system_vsync_interval = 1;
    g_game_state = GAME_STATE_ACTION_EXECUTE_START;
    unit = unit_casting_get();
    unit->state_frame_counter = 0;
    if (unit->continue_attack_count == 0) {
        unit_facing_turn_to_target(unit, 0);
    }
    action = &unit->strike;
    if (action->target_count != 0) {
        for (i = 0; i < action->target_count; i++) {
            target = unit_battle_get(action->target_list[i]);
            if (target != 0) {
                target->pending_attack_result = action_apply_target_result(target->stats->battle_id);
                if (target->pending_attack_result == -1) {
                    unit_find_relocation_tile(target->stats->battle_id, &target->dismount);
                }
            }
        }
    }

    if (g_action_phase == ACTION_PHASE_ACTING_UNIT) {
        if (unit->used_ability_id != 0 && !IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)
            && action->reaction_occurred == 0) {
            animation_ability_attack_select(unit, unit_battle_get(action->target_list[0]));
        } else if (unit->used_ability_id == 0 || IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)) {
            if (action->target_count != 0) {
                animation_weapon_attack_select(unit, unit_battle_get(action->target_list[0]));
            } else {
                animation_weapon_attack_select(unit, 0);
            }
        }
    } else if (unit->used_ability_id == ABILITY_ID_KNOCKBACK) {
        for (i = 0; i < unit->strike.target_count; i++) {
            target = unit_battle_get(action->target_list[i]);
            if (target != 0) {
                target->map_x = unit->strike.target_tile.x;
                target->map_y = unit->strike.target_tile.y;
                target->map_layer = unit->strike.target_tile.layer;
                unit_set_tile_position(target->stats->battle_id, target->map_x, target->map_y, target->map_layer,
                    target->facing / UNIT_FACING_QUARTER_TURN);
                unit_set_move_and_world_coords(target);
            }
        }
    } else if (action->reaction_ability_id != ABILITY_ID_REACTION_REFLECT) {
        if (unit->used_ability_id != 0 && !IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)
            && unit->continue_attack_count == 0) {
            if (action->reaction_occurred == 0) {
                animation_ability_attack_select(unit, unit_battle_get(action->target_list[0]));
            }
        } else if (unit->used_ability_id == 0 || IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)) {
            if (action->target_count != 0) {
                animation_weapon_attack_select(unit, unit_battle_get(action->target_list[0]));
            } else {
                animation_weapon_attack_select(unit, 0);
            }
        }
    }
}

/* Resolve the casting unit's next strike and start showing it.
 *
 * When a reaction replaced the ability, or the strike is a plain attack, the
 * knockback strike or an ability without an effect file, the attack animation
 * starts directly. Otherwise the game enters the effect-file-open state with the
 * targets and their result animations stored for the effect. */
void action_start_next_strike(void) {
    unit_runtime_t* unit;
    u16 attack_id;
    s32 ability;
    s32 reaction;

    g_system_vsync_interval = 1;
    unit = unit_casting_get();
    action_resolve_strike(unit->stats->battle_id, &unit->strike);
    attack_id = unit->strike.last_attack_id;
    /* The target reads reaction_id as a signed 16-bit value. */
    reaction = unit->strike.reaction_id;
    ability = attack_id;
    unit->used_ability_id = attack_id;
    sprite_thrown_item_palette_set(unit->strike.used_weapon_id, unit);
    if (((reaction != 0) && (ability != reaction)) || (ability == ABILITY_ID_KNOCKBACK) || (ability == 0)
        || (effect_ability_has_no_file(ability) != 0)) {
        action_apply_strike_results();
        return;
    }
    g_game_state = GAME_STATE_START_EFFECT_FILE_OPEN;
    action_effect_start(ability, unit);
}

/* Enter the OpenSp2Files game state at vsync interval 1, with no SP2 buffer yet, so its
 * handler starts a fresh load. */
void state_enter_open_sp2_files(void) {
    g_system_vsync_interval = 1;
    g_game_state = GAME_STATE_OPEN_SP2_FILES;
    g_sprite_sp2_data = 0;
}

/* Enter the pre-attack animation state: clear the tile marks and start the used
 * ability's charge animation, or the standing animation when the ability has no
 * charge animation set. */
void state_enter_pre_attack_animation(void) {
    unit_runtime_t* unit;
    u16 ability;

    target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 2);
    g_game_state = GAME_STATE_PRE_ATTACK_ANIMATION;
    unit = unit_source_get();
    ability = unit->used_ability_id;
    unit->state_frame_counter = 0;
    if (g_ability_animation_data[ability].charge_animation_set_id != 0) {
        animation_ability_charge_start(unit);
    } else {
        /* facing is read signed (lh) here. */
        animation_facing_store(ANIMATION_STAND, unit->facing, unit);
    }
    cursor_hide();
}

/* Enter the action-cast state for the source unit, unless the scenario check
 * starts an event first; the camera turns when the unit's tile is hidden. */
void state_enter_action_cast(void) {
    unit_runtime_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_ACTION_CAST;
    g_state_window_closed = 0;
    unit = unit_source_get();
    if (event_try_enter_scenario(unit) == 0) {
        g_game_state = GAME_STATE_ACTION_CAST;
        camera_unit_visibility_rotate(unit);
    }
}

/* Starts the acting unit's confirmed ability: marks the action taken, faces the
 * cursor and commits the attack preparation. An immediate or Jump action enters
 * the commence-attack phase, with the charge-hold animation when the ability has a
 * charge set. A charged action starts its charge-hold animation and ends the
 * command until the charge comes due; any other result raises a pointer
 * exception and ends the command. */
void action_execute_ability(void) {
    unit_runtime_t* unit;
    s32 result;

    unit_view_start(UNIT_VIEW_MODE_NONE, 0, 0);
    unit = unit_source_get();
    turn_act_taken_set(unit->stats->battle_id);
    unit->turn_action_flags |= TURN_ACTION_ACTED;
    unit_facing_turn_to_cursor(unit);
    result = action_commit_preparation(&unit->command_state.command.data.action);
    unit->attack_phase_state = result;
    switch (result) {
    case ACTION_PREPARATION_CHARGE:
        animation_ability_charge_hold_start(unit);
        break;
    case ACTION_PREPARATION_IMMEDIATE:
    case ACTION_PREPARATION_JUMP:
        camera_pan_disable();
        g_game_state = GAME_STATE_COMMENCE_ATTACK_PHASE;
        g_state_window_closed = 0;
        unit->turn_action_flags |= TURN_ACTION_ACTED;
        if (g_ability_animation_data[unit->used_ability_id].charge_animation_set_id != 0) {
            animation_ability_charge_hold_start(unit);
        }
        return;
    case ACTION_PREPARATION_CHANGE_EQUIPMENT:
    default:
        system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_ATTACK_PREPARATION);
        break;
    }
    action_end_command(unit);
}

/* Enter the commence-attack phase for the source unit: copy the action command stored in its
 * battle stats into its runtime action, mark it acted and start the charge-hold animation when it
 * uses an ability. */
void state_enter_commence_attack_phase(void) {
    unit_stats_t* stats;
    unit_runtime_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_COMMENCE_ATTACK_PHASE;
    unit = unit_source_get();
    stats = unit->stats;
    unit->command_state.command.data.action = stats->action_command;
    /* action_command.ability_id is s16 in the header; the target loads it with lhu. */
    unit->used_ability_id = unit->stats->action_command.ability_id;
    unit->turn_action_flags |= TURN_ACTION_ACTED;
    if (unit->used_ability_id != 0) {
        animation_ability_charge_hold_start(unit);
    }
}

/* Enter the ability-preview-help state and start the help menu. Nothing in the
 * game calls it. */
void state_enter_ability_preview_help(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_ABILITY_PREVIEW_HELP;
    g_state_in_help = 1;
    help_menu_thread_start();
}

/*
 * Seed the source unit's pending action target and run target-tile selection.
 *
 * An action without targeting (TARGET_PANELS_NO_TARGETING) targets the unit's own
 * tile; otherwise a player-controlled unit targets the cursor tile and an AI unit
 * keeps the target it chose. target_area_set_for_action's result picks
 * the next step: 0 or 1 (affected tiles marked or not) enter the ability-preview
 * state with the free cursor, 2 (a menu type without targeting) goes straight to
 * the pre-attack animation, and -1 (an invalid action) returns to the idle action
 * menu.
 */
void target_tile_select(void) {
    unit_runtime_t* unit = unit_source_get();
    s32 result;
    s32 saved_input_mode;

    unit->state_frame_counter = 0;
    if (unit->ability_preview_phase == TARGET_PANELS_NO_TARGETING) {
        vector_short_set(
            &unit->command_state.command.data.action.target_position, unit->map_x, unit->map_layer, unit->map_y);
        unit->command_state.command.data.action.targeting_type = TARGET_TYPE_TILE;
    } else if (unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) {
        vector_short_set(
            &unit->command_state.command.data.action.target_position, g_cursor_x, g_cursor_layer, g_cursor_y);
        unit->command_state.command.data.action.targeting_type = TARGET_TYPE_TILE;
    }
    result = target_area_set_for_action(&unit->command_state.command.data.action);
    unit->target_select_result = result;
    switch (result) {
    case 0:
    case 1:
        camera_pan_enable();
        saved_input_mode = g_cursor_input_mode;
        g_game_state = GAME_STATE_ABILITY_PREVIEW_HANDLING;
        g_cursor_input_mode = CURSOR_INPUT_FREE_CURSOR;
        g_cursor_input_mode_saved = saved_input_mode;
        target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_SHOW_TARGETS, 3);
        cursor_show_attack_preview_panel();
        if (g_game_options.fields.target_flashing == GAME_OPTION_ON) {
            sprite_palette_tint_all_by_team();
        }
        break;
    case 2:
        camera_pan_disable();
        state_enter_pre_attack_animation();
        cursor_show_attack_preview_panel();
        break;
    case -1:
        menu_action_dispatch();
        break;
    }
    cursor_hide();
}

/* Enter the illegal-range state, reached when the chosen tile lacks the
 * ability-range mark: opens action-confirm option 5 for the source unit. */
void state_enter_illegal_range(void) {
    unit_runtime_t* unit;
    unit_stats_t* stats;

    camera_pan_disable();
    g_game_state = GAME_STATE_ILLEGAL_RANGE;
    unit = unit_source_get();
    stats = unit->stats;
    menu_request_dispatch(
        MENU_REQUEST_ACTION_CONFIRM, 5, stats->battle_id, 0, unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Start target selection for the source unit's previewed action and return its
 * ability_preview_phase (target_panels_e).
 *
 * An action with a target to choose enters the targeting-range state with the
 * range tiles shown and the cursor unit as preview target. An action without
 * targeting selects its tile at once. With no valid panels the idle action menu
 * reopens. */
target_panels_e target_tile_selection_start(void) {
    unit_runtime_t* unit = unit_source_get();

    unit->state_frame_counter = 0;
    switch (unit->ability_preview_phase) {
    case TARGET_PANELS_UNIT_OR_TILE:
    case TARGET_PANELS_TILE_ONLY:
        camera_pan_disable();
        g_game_state = GAME_STATE_TARGETING_RANGE;
        target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_SHOW_RANGE, 2);
        cursor_show_right_unit_panel();
        if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
            cursor_show();
        } else {
            cursor_hide();
        }
        break;
    case TARGET_PANELS_NO_TARGETING:
        camera_pan_disable();
        target_tile_select();
        cursor_show_right_unit_panel();
        break;
    case TARGET_PANELS_INVALID:
    case TARGET_PANELS_NONE:
        menu_action_dispatch();
        break;
    }
    return unit->ability_preview_phase;
}

/* Enter the ConfirmAction state and open the action-confirm prompt for the source unit. The prompt
 * option depends on how the target is chosen and on whether a unit stands on the cursor tile: 1
 * for a unit an ability can follow, 4 for a unit under a tile-only ability, 3 for a unit with a
 * self-centered action, 2 for an empty tile and 0 otherwise. */
void state_enter_confirm_action(void) {
    unit_runtime_t* unit;
    unit_runtime_t* target;

    camera_pan_disable();
    g_game_state = GAME_STATE_CONFIRM_ACTION;
    unit = unit_source_get();
    target = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
    switch (unit->ability_preview_phase) {
    case TARGET_PANELS_UNIT_OR_TILE:
        if (target != 0) {
            menu_request_dispatch(MENU_REQUEST_ACTION_CONFIRM, 1, unit->stats->battle_id, 0,
                unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            menu_request_dispatch(MENU_REQUEST_ACTION_CONFIRM, 2, unit->stats->battle_id, 0,
                unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        }
        break;
    case TARGET_PANELS_TILE_ONLY:
        if (target != 0) {
            menu_request_dispatch(MENU_REQUEST_ACTION_CONFIRM, 4, unit->stats->battle_id, 0,
                unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            menu_request_dispatch(MENU_REQUEST_ACTION_CONFIRM, 2, unit->stats->battle_id, 0,
                unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        }
        break;
    case TARGET_PANELS_NO_TARGETING:
        if (target != 0) {
            menu_request_dispatch(MENU_REQUEST_ACTION_CONFIRM, 3, unit->stats->battle_id, 0,
                unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            menu_request_dispatch(MENU_REQUEST_ACTION_CONFIRM, 2, unit->stats->battle_id, 0,
                unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        }
        break;
    default:
        menu_request_dispatch(MENU_REQUEST_ACTION_CONFIRM, 0, unit->stats->battle_id, 0,
            unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        break;
    }
}

/* Starts the repeating tint that marks the candidates of an auto-battle target
 * choice: red on every unit of another team than the casting unit's for
 * TURN_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT, blue on every unit of its own
 * team for TURN_COMMAND_AUTOBATTLE_PROTECT_ALLIES_SELECT. Other modes tint
 * nothing. */
static void palette_blue_or_red_highlight(s32 mode) {
    unit_runtime_t* caster = unit_casting_get();
    unit_runtime_t* unit;
    s32 i;

    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        unit = unit_runtime_get(i);
        if (unit == 0) {
            continue;
        }
        if (unit->stats == 0) {
            continue;
        }
        switch (mode) {
        case TURN_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT:
            if ((unit->stats->team_flags & UNIT_TEAM_MASK) != (caster->stats->team_flags & UNIT_TEAM_MASK)) {
                battle_map_palette_modify(
                    BATTLE_MAP_COLOR_MODE_BASE_REPEAT, 4, BATTLE_MAP_CLUT_ROW_UNITS, i, 0, 0x10, 0, 0);
            }
            continue;
        case TURN_COMMAND_AUTOBATTLE_PROTECT_ALLIES_SELECT:
            if ((unit->stats->team_flags & UNIT_TEAM_MASK) == (caster->stats->team_flags & UNIT_TEAM_MASK)) {
                battle_map_palette_modify(
                    BATTLE_MAP_COLOR_MODE_BASE_REPEAT, 4, BATTLE_MAP_CLUT_ROW_UNITS, i, 0, 0, 0, 0x18);
            }
            continue;
        }
    }
}

/* Enter the target-select-start state: open the targeting prompt for the casting
 * unit. */
static void target_selection_start_enter(void) {
    unit_runtime_t* unit;
    unit_stats_t* stats;

    camera_pan_disable();
    g_game_state = GAME_STATE_TARGET_SELECT_START;
    unit = unit_casting_get();
    stats = unit->stats;
    menu_request_dispatch(MENU_REQUEST_TARGETING_PROMPT, MENU_TARGETING_PROMPT_DEFAULT, stats->battle_id, 0, 1);
    cursor_show();
}

/* Enter the target-select state: show the panel for the cursor tile and tint the
 * units the casting unit's selection command may pick, the other teams' red for a
 * targeted selection and its own team's blue for a recovery selection. */
void state_enter_target_select(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_TARGET_SELECT;
    cursor_show_hovered_unit_panel();
    palette_blue_or_red_highlight(unit_casting_get()->target_select_command);
}

/* Enter the target-select-denied state for the cursor tile: show the summary of a
 * unit standing there and open the refusal variant of the target-confirm prompt
 * for the casting unit.
 *
 * Option 3 is used for an empty tile, 1 for a targeted auto-battle selection and
 * 2 for a recovery selection. Any other selection command over a unit opens no
 * prompt. */
void state_enter_target_select_denied(void) {
    unit_runtime_t* occupant;
    unit_runtime_t* caster;
    unit_stats_t* stats;
    s32 command;

    camera_pan_disable();
    g_game_state = GAME_STATE_TARGET_SELECT_DENIED;
    occupant = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
    if (occupant != 0) {
        stats = occupant->stats;
        if (stats != 0) {
            unit_view_start(UNIT_VIEW_MODE_UNIT_SUMMARY, stats->battle_id, 0);
        }
    }
    caster = unit_casting_get();
    if (occupant == 0) {
        menu_request_dispatch(MENU_REQUEST_TARGET_CONFIRM, 3, caster->stats->battle_id, 0, 1);
        return;
    }
    command = caster->target_select_command;
    if (command == TURN_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT) {
        menu_request_dispatch(MENU_REQUEST_TARGET_CONFIRM, 1, caster->stats->battle_id, 0, 1);
        return;
    }
    if (command == TURN_COMMAND_AUTOBATTLE_PROTECT_ALLIES_SELECT) {
        menu_request_dispatch(MENU_REQUEST_TARGET_CONFIRM, 2, caster->stats->battle_id, 0, 1);
    }
}

/* Enter the target-select-confirm state: show the summary of the unit under the
 * cursor and open the target-confirm prompt (option 0) for the casting unit. */
void state_enter_target_select_confirm(void) {
    unit_runtime_t* unit;
    unit_stats_t* stats;

    camera_pan_disable();
    g_game_state = GAME_STATE_TARGET_SELECT_CONFIRM;
    unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
    if (unit != 0) {
        stats = unit->stats;
        if (stats != 0) {
            unit_view_start(UNIT_VIEW_MODE_UNIT_SUMMARY, stats->battle_id, 0);
        }
    }
    stats = unit_casting_get()->stats;
    menu_request_dispatch(MENU_REQUEST_TARGET_CONFIRM, 0, stats->battle_id, 0, 1);
}

/* Enter the target-display-start state: show the tile info panel and open the
 * targeting prompt for the casting unit. */
static void target_display_start_enter(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_TARGET_DISPLAY_START;
    unit_view_start(UNIT_VIEW_MODE_TILE_INFO, 0xFF, 0xFF);
    menu_request_dispatch(
        MENU_REQUEST_TARGETING_PROMPT, MENU_TARGETING_PROMPT_DEFAULT, unit_casting_get()->stats->battle_id, 0, 1);
    g_state_window_closed = 0;
    cursor_show();
}

/* Enter the target-display state: move the cursor to the casting unit's
 * auto-battle target, turn the camera when its tile is hidden and show its
 * summary panel. */
void state_enter_target_display(void) {
    unit_stats_t* caster_stats;
    unit_runtime_t* target;
    unit_stats_t* target_stats;

    camera_pan_disable();
    g_game_state = GAME_STATE_TARGET_DISPLAY;
    caster_stats = unit_casting_get()->stats;
    target = unit_battle_get(caster_stats->auto_battle_target);
    cursor_move_to_unit(target);
    camera_unit_visibility_rotate(target);
    if (target != 0) {
        target_stats = target->stats;
        if (target_stats != 0) {
            unit_view_start(UNIT_VIEW_MODE_UNIT_SUMMARY, target_stats->battle_id, 0);
        }
    }
}

/* Enter the Effect game state at two vertical blanks per frame. No code calls it
 * or state_enter_effect_playback, the only other entry to that state. */
void state_enter_effect(void) {
    g_system_vsync_interval = 2;
    g_game_state = GAME_STATE_EFFECT;
    cursor_show();
}

/* Enter the MapJumpingOut game state at two vertical blanks per frame. No code
 * calls it. */
void state_enter_map_jumping_out(void) {
    g_system_vsync_interval = 2;
    g_game_state = GAME_STATE_MAP_JUMPING_OUT;
}

/* Enter the MapJumpingIn game state at two vertical blanks per frame. No code
 * calls it. */
void state_enter_map_jumping_in(void) {
    g_system_vsync_interval = 2;
    g_game_state = GAME_STATE_MAP_JUMPING_IN;
}

/* Queue the CloseBattle game state; callers always pass transition step 8. */
void state_enter_close_battle(s32 transition_step, system_game_flow_e close_flow_state) {
    g_system_vsync_interval = 2;
    g_game_state = GAME_STATE_CLOSE_BATTLE;
    g_fade_step = transition_step;
    g_game_flow_state = close_flow_state;
}

/* Project the cursor tile to the screen; while g_cursor_visible is set, also draw
 * the map cursor and its tile glow. */
void cursor_update(void) {
    SVECTOR position;
    SVECTOR raised;
    VECTOR projected;

    if (g_cursor_visible != 0) {
        cursor_tile_glow_draw();
        return;
    }
    cursor_tile_project(&projected, &position, &raised);
}

/* Per-frame unit update shared by most game-state handlers; it reads no input.
 *
 * Projects the units, animates them unless the status screen owns the frame,
 * updates the status bubbles and unit graphics, and projects the cursor tile,
 * drawing the map cursor and tile glow while g_cursor_visible is
 * set. */
void state_update_units(void) {
    sprite_units_project();
    if (g_menu_status_screen_selected != 1) {
        unit_animations_update();
    }
    sprite_units_display_update();
    cursor_update();
}

/* Free-cursor game state: camera and cursor input, then the button actions
 * (help, team highlight, act menu, move range, AT list and mini menu). */
void state_handle_free_cursor(void) {
    unit_runtime_t* unit;
    s32 input;
    RECT rect; /* never used; retail reserves its 8 bytes */

    camera_rotation_handle_input();
    camera_zoom_handle_input();
    camera_tilt_handle_input();
    cursor_move_by_input();
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    input = g_input_pressed_buttons;
    if (input & PSX_PAD_SELECT) {
        at_list_reset_cursor();
        state_enter_free_cursor_help();
    } else if (input & PSX_PAD_SQUARE) {
        at_list_reset_cursor();
        state_enter_highlight_units();
    } else if (input & PSX_PAD_CIRCLE) {
        target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 0);
        sound_sfx_play(SOUND_SFX_CONFIRM);
        if (unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer) != 0) {
            menu_action_dispatch();
        } else {
            unit = unit_source_get();
            if (unit != 0) {
                cursor_move_to_unit(unit);
                state_enter_open_action_menus();
            }
        }
    } else if (input & PSX_PAD_CROSS) {
        at_list_reset_cursor();
        unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
        if (unit != 0) {
            g_unit_casting_runtime_id = unit->runtime_id;
            state_enter_display_move_area();
        }
    } else if (input & PSX_PAD_START) {
        if (g_game_options.fields.multi_height_cursor_speed != GAME_MULTI_HEIGHT_CURSOR_SPEED_STOP) {
            sound_sfx_play(SOUND_SFX_CONFIRM);
            at_list_show_next_entry();
            cursor_show_hovered_unit_panel();
        }
    } else if (input & PSX_PAD_TRIANGLE) {
        at_list_reset_cursor();
        sound_sfx_play(SOUND_SFX_CONFIRM);
        state_enter_mini_menu();
    }
}

/* FreeCursorHelp game-state handler: when the help window has gone, return to the
 * free cursor. */
void state_handle_free_cursor_help(void) {
    if (menu_thread_activity_get() != 2) {
        g_state_in_help = 0;
        state_enter_free_cursor();
    }
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
}

/* HighlightUnits game-state handler: releasing square restores the cursor input
 * mode and the unit palettes and returns to the free cursor. */
void state_handle_highlight_units(void) {
    if (g_input_released_buttons & PSX_PAD_SQUARE) {
        g_cursor_input_mode = g_cursor_input_mode_saved;
        state_enter_free_cursor();
        sprite_palette_restore_all();
    }
    camera_rotation_handle_input();
    camera_zoom_handle_input();
    camera_tilt_handle_input();
    state_update_units();
}

/* OpenActionMenus game-state handler: once the window has closed (any answer) and the camera has
 * settled, open the idle action menu of the unit under the cursor. */
void state_handle_open_action_menus(void) {
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= TURN_COMMAND_PROMPT_YES
        && (selected_command <= TURN_COMMAND_PROMPT_NO || selected_command == TURN_COMMAND_UNRESOLVED_FF)) {
        g_state_window_closed = 1;
    }
    if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_state_window_closed != 0) {
        menu_action_dispatch();
    }
}

/* IdlingActionMenus game-state handler: act on the selected menu command.
 *
 * Move (0) hands off to the move command, moving the unit's mount partner
 * instead when it has one, and its MOVE_SELF variant moves the unit itself; Act (1)
 * dispatches Defend, Equip Change and ordinary abilities on the command's
 * skillset, Wait (2) chooses a facing, and the remaining commands enter
 * target selection/display, the event check or scripted menus.
 *
 * The empty cases for the unresolved commands 0xfe and 0xff keep the switch a
 * compare tree: the two-value range above 0x19 disables the jump table
 * (range > 10 * count). */
void state_handle_idling_action_menus(void) {
    turn_command_t* command;
    unit_runtime_t* unit;
    s32 result;

    if (menu_thread_activity_get() == 0) {
        state_enter_free_cursor();
    }
    if (g_input_pressed_buttons & PSX_PAD_SELECT) {
        state_enter_action_help_menu();
    }
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    command = command_get_selected();
    unit = unit_casting_get();
    switch (command->kind) {
    case TURN_COMMAND_MOVE:
        if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
            g_unit_casting_runtime_id = unit_runtime_get(unit->mount_partner_runtime_id)->runtime_id;
        } else {
            g_unit_casting_runtime_id = g_unit_source_runtime_id;
        }
        at_list_reset_cursor();
        state_enter_unit_move();
        break;
    case TURN_COMMAND_MOVE_SELF:
        g_unit_casting_runtime_id = g_unit_source_runtime_id;
        at_list_reset_cursor();
        state_enter_unit_move();
        break;
    case TURN_COMMAND_ACT:
        at_list_reset_cursor();
        switch (command->data.action.skillset_id) {
        case SKILLSET_ID_DEFEND:
            unit->command_state.command = *command;
            status_inflict_defending(unit->stats->battle_id);
            unit->turn_action_flags |= TURN_ACTION_ACTED;
            unit_runtime_display_update(unit->runtime_id);
            turn_act_taken_set(unit->stats->battle_id);
            state_enter_after_command();
            break;
        case SKILLSET_ID_EQUIP_CHANGE:
            unit->command_state.command = *command;
            unit->turn_action_flags |= TURN_ACTION_ACTED;
            status_clear_action_state(unit->stats);
            ai_unit_abilities_init(unit->stats->battle_id);
            unit_runtime_display_update(unit->runtime_id);
            turn_act_taken_set(unit->stats->battle_id);
        case SKILLSET_ID_BLANK_04:
        case SKILLSET_ID_MIMIC:
            state_enter_after_command();
            break;
        case SKILLSET_ID_ATTACK:
        default:
            unit->command_state.command = *command;
            unit->command_state.command.data.action.actor_id = unit->stats->battle_id;
            unit->command_state.command.data.action.targeting_type = TARGET_TYPE_TILE;
            action_prepare_in_place(&unit->command_state.command.data.action);
            unit->used_ability_id = unit->command_state.command.data.action.ability_id;
            result = target_range_set_for_action(&unit->command_state.command.data.action);
            unit->ability_preview_phase = result;
            switch (result) {
            case TARGET_PANELS_UNIT_OR_TILE:
            case TARGET_PANELS_TILE_ONLY:
            case TARGET_PANELS_NONE:
                state_enter_targeting_prompt();
                break;
            case TARGET_PANELS_NO_TARGETING:
            default:
                target_tile_select();
                break;
            }
            break;
        }
        break;
    case TURN_COMMAND_END_TURN:
        state_enter_wait_menu();
        break;
    case TURN_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT:
    case TURN_COMMAND_AUTOBATTLE_PROTECT_ALLIES_SELECT:
        unit->target_select_command = command->kind;
        target_selection_start_enter();
        break;
    case TURN_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_DISPLAY:
    case TURN_COMMAND_AUTOBATTLE_PROTECT_ALLIES_DISPLAY:
        unit->target_select_command = command->kind;
        target_display_start_enter();
        break;
    case TURN_COMMAND_AUTOBATTLE_FADING_LIFE:
    case TURN_COMMAND_AUTOBATTLE_RETREAT:
        state_enter_open_action_menus();
        break;
    case TURN_COMMAND_NO_MENU_RECORD:
        menu_status_clear();
        turn_next_event_start();
        break;
    case TURN_COMMAND_REFRESH_CONTROL:
        unit_store_team_flags(unit);
        break;
    case TURN_COMMAND_UNRESOLVED_FE:
    case TURN_COMMAND_UNRESOLVED_FF:
        break;
    }
}

/*
 * AiCommand game-state handler for a unit the AI controls. From the
 * unit's 31st frame in the state it builds the unit's AI command, at half speed
 * until ai_command_build reports the command ready (it can take several
 * frames), then carries the command out as the menu handler would: a move, an
 * act (Defend, Equip Change, Mimic, or an ability through target selection) or
 * the end of the turn, which marks both move and act taken.
 *
 * Each special skillset's completion stays explicit so its distinct call tail
 * is preserved.
 */
void state_handle_ai_command(void) {
    unit_runtime_t* unit;
    s32 kind;
    s32 result;
    u16 frame;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    unit = unit_source_get();
    if (unit->state_frame_counter >= 31 && !unit->command_ready) {
        g_system_vsync_interval = 2;
        if (!ai_command_build(unit->stats->battle_id, &unit->command_state.command)) {
            if (unit->command_state.command.kind == TURN_COMMAND_ACT) {
                unit->command_state.command.data.action.actor_id = unit->stats->battle_id;
                action_prepare_in_place(&unit->command_state.command.data.action);
            }
            unit->command_ready = 1;
            g_system_vsync_interval = 1;
        }
    }
    frame = unit->state_frame_counter++;
    if (frame < 31 || !unit->command_ready)
        return;
    kind = unit->command_state.command.kind;
    switch (kind) {
    case TURN_COMMAND_MOVE:
        if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
            g_unit_casting_runtime_id = unit_runtime_get(unit->mount_partner_runtime_id)->runtime_id;
            state_enter_unit_move();
            return;
        }
        /* fallthrough */
    case TURN_COMMAND_MOVE_SELF:
        g_unit_casting_runtime_id = g_unit_source_runtime_id;
        state_enter_unit_move();
        return;
    case TURN_COMMAND_ACT:
        switch (unit->command_state.command.data.action.skillset_id) {
        case SKILLSET_ID_DEFEND:
            status_inflict_defending(unit->stats->battle_id);
            unit->turn_action_flags |= TURN_ACTION_ACTED;
            unit_runtime_display_update(unit->runtime_id);
            turn_act_taken_set(unit->stats->battle_id);
            state_enter_after_command();
            return;
        case SKILLSET_ID_EQUIP_CHANGE:
            unit->turn_action_flags |= TURN_ACTION_ACTED;
            status_clear_action_state(unit->stats);
            ai_unit_abilities_init(unit->stats->battle_id);
            unit_runtime_display_update(unit->runtime_id);
            turn_act_taken_set(unit->stats->battle_id);
            state_enter_after_command();
            return;
        case SKILLSET_ID_BLANK_04:
        case SKILLSET_ID_MIMIC:
            turn_act_taken_set(unit->stats->battle_id);
            state_enter_after_command();
            return;
        /* Explicit ordinary skillsets retain the native 24-entry dispatch table. */
        case SKILLSET_ID_ATTACK:
        case SKILLSET_ID_BASIC_SKILL:
        case SKILLSET_ID_ITEM:
        case SKILLSET_ID_BATTLE_SKILL:
        case SKILLSET_ID_CHARGE:
        case SKILLSET_ID_PUNCH_ART:
        case SKILLSET_ID_WHITE_MAGIC:
        case SKILLSET_ID_BLACK_MAGIC:
        case SKILLSET_ID_TIME_MAGIC:
        case SKILLSET_ID_SUMMON_MAGIC:
        case SKILLSET_ID_STEAL:
        case SKILLSET_ID_TALK_SKILL:
        case SKILLSET_ID_YIN_YANG_MAGIC:
        case SKILLSET_ID_ELEMENTAL:
        case SKILLSET_ID_JUMP:
        case SKILLSET_ID_DRAW_OUT:
        case SKILLSET_ID_THROW:
        case SKILLSET_ID_MATH_SKILL:
        case SKILLSET_ID_SING:
        case SKILLSET_ID_DANCE:
        default:
            unit->used_ability_id = unit->command_state.command.data.action.ability_id;
            result = target_range_set_for_action(&unit->command_state.command.data.action);
            unit->ability_preview_phase = result;
            switch (result) {
            case TARGET_PANELS_UNIT_OR_TILE:
            case TARGET_PANELS_TILE_ONLY:
            case TARGET_PANELS_NONE:
                state_enter_targeting_prompt();
                return;
            case TARGET_PANELS_NO_TARGETING:
            default:
                target_tile_select();
                return;
            }
        }
    case TURN_COMMAND_END_TURN:
        break;
    default:
        return;
    }
    unit->turn_action_flags |= TURN_ACTION_MOVED | TURN_ACTION_ACTED;
    turn_end(unit->stats->battle_id);
    state_enter_after_command();
}

/* ActionHelpMenu game-state handler: when the help window has gone, reopen the
 * idle action menu, the active unit's when the casting unit is the source unit
 * and the non-active unit's otherwise. */
void state_handle_action_help_menu(void) {
    unit_runtime_t* unit;

    if (menu_thread_activity_get() != 2) {
        g_state_in_help = 0;
        unit = unit_casting_get();
        if (unit != 0) {
            if (unit->runtime_id == g_unit_source_runtime_id) {
                menu_action_open_active_unit();
            } else {
                menu_action_open_non_active_unit();
            }
        }
    }
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
}

/* DisplayMoveArea game-state handler, a unit's move range shown from the free
 * cursor: cross clears the marks, restores the cursor input mode and returns to
 * the free cursor. */
void state_handle_display_move_area(void) {
    if (g_input_pressed_buttons & PSX_PAD_CROSS) {
        target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 1);
        g_cursor_input_mode = g_cursor_input_mode_saved;
        sound_sfx_play(SOUND_SFX_CANCEL);
        state_enter_free_cursor();
    }
    camera_rotation_handle_input();
    camera_zoom_handle_input();
    camera_tilt_handle_input();
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
}

/* MiniMenu game-state handler. Select opens the help window. A menu result of 0
 * (closed) restores the cursor movement option and returns to the free cursor; a
 * result of 0x64 or more shows the unit of AT-list entry result - 0x64. */
void state_handle_mini_menu(void) {
    s32 menu_result = menu_thread_activity_get();

    if (g_input_pressed_buttons & PSX_PAD_SELECT) {
        state_enter_mini_menu_help();
    } else if (menu_result == 0) {
        g_cursor_input_mode = g_game_options.fields.cursor_movement;
        state_enter_free_cursor();
    } else if (menu_result >= 0x64) {
        at_list_show_entry(menu_result - 0x64);
    }
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
}

/* MiniMenuHelp game-state handler: when the help window has gone, reopen the mini
 * menu. */
void state_handle_mini_menu_help(void) {
    if (menu_thread_activity_get() != 2) {
        g_state_in_help = 0;
        state_enter_mini_menu();
    }
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
}

/* Inspect the unit or charged action selected from the AT List.
 * Circle or Cross returns to the mini menu. */
void state_handle_at_list_inspection(void) {
    s32 controller_input = g_input_pressed_buttons;

    if ((controller_input & PSX_PAD_CIRCLE) || (controller_input & PSX_PAD_CROSS)) {
        state_return_to_mini_menu();
    }
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
}

/* UnitMove game-state handler: YES goes on to choosing a destination
 * (state_enter_select_move_tile); NO or UNRESOLVED_FF returns to the idle action menu. */
void state_handle_unit_move(void) {
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    switch (selected_command) {
    case TURN_COMMAND_PROMPT_YES:
        state_enter_select_move_tile();
        return;
    case TURN_COMMAND_PROMPT_NO:
    case TURN_COMMAND_UNRESOLVED_FF:
        menu_action_open_active_unit();
        return;
    }
}

/* SelectMoveTile game-state handler: choosing the move destination. The casting
 * unit is the one that walks (the source unit, or its mount).
 *
 * A player-controlled source unit moves the cursor and camera. Circle paths the
 * casting unit to the cursor tile and starts the move setup, or opens the
 * illegal-move help when no path reaches it; cross clears the move range and
 * returns to the idle action menu; triangle recenters the cursor on the unit.
 * An AI-controlled unit waits 31 frames, then paths to the tile its AI command
 * chose, puts the cursor there and starts the move setup, or goes on to its next
 * scripted menu when no path reaches it. */
void state_handle_select_move_tile(void) {
    unit_runtime_t* source = unit_source_get();
    unit_runtime_t* casting = unit_casting_get();
    move_path_t* path;
    s32 buttons;
    u16 frame;
    unit_command_state_t scratch; /* never used; retail reserves its frame space */

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    if (source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) {
        cursor_move_by_input();
        camera_rotation_handle_input();
        camera_zoom_handle_input();
        camera_tilt_handle_input();
        buttons = g_input_pressed_buttons;
        if (buttons & PSX_PAD_CIRCLE) {
            path = move_path_build_to_tile(casting->stats->battle_id, g_cursor_x, g_cursor_y, g_cursor_layer);
            if (path != 0) {
                casting->path = *path;
                casting->walk_speed = 0x2000;
                target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 1);
                sound_sfx_play(SOUND_SFX_CONFIRM);
                state_enter_unit_moving_setup();
                return;
            }
            state_enter_illegal_move_menu();
            return;
        }
        if (buttons & PSX_PAD_CROSS) {
            target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 1);
            cursor_move_to_unit(source);
            menu_action_open_active_unit();
            return;
        }
        if (buttons & PSX_PAD_TRIANGLE) {
            cursor_move_to_unit(unit_source_get());
            cursor_show_hovered_unit_panel();
        }
        return;
    }
    frame = source->state_frame_counter;
    source->state_frame_counter = frame + 1;
    if (frame < 0x1f) {
        return;
    }
    path = move_path_build_to_tile(casting->stats->battle_id, source->command_state.command.data.move.x,
        source->command_state.command.data.move.y, source->command_state.command.data.move.layer);
    target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 1);
    if (path != 0) {
        g_cursor_x = source->command_state.command.data.move.x;
        g_cursor_layer = source->command_state.command.data.move.layer;
        g_cursor_y = source->command_state.command.data.move.y;
        casting->path = *path;
        casting->walk_speed = 0x2000;
        state_enter_unit_moving_setup();
        return;
    }
    state_enter_open_action_menus();
}

/* MoveRangeException game-state handler: when the message closes (any answer), reopen the idle
 * action menu of the unit under the cursor. */
void state_handle_move_range_exception(void) {
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= TURN_COMMAND_PROMPT_YES
        && (selected_command <= TURN_COMMAND_PROMPT_NO || selected_command == TURN_COMMAND_UNRESOLVED_FF)) {
        menu_action_dispatch();
    }
}

/* IllegalMoveMenu game-state handler: when the message closes (any answer), return to choosing a
 * destination (state_enter_select_move_tile). */
void state_handle_illegal_move_menu(void) {
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= TURN_COMMAND_PROMPT_YES
        && (selected_command <= TURN_COMMAND_PROMPT_NO || selected_command == TURN_COMMAND_UNRESOLVED_FF)) {
        state_enter_select_move_tile();
    }
}

/* UnitMovingSetup game-state handler: YES starts the move; NO or UNRESOLVED_FF discards the path
 * and returns to choosing a destination. */
void state_handle_unit_moving_setup(void) {
    turn_command_t* selected_command_address;
    s32 selected_command;
    unit_runtime_t* casting_unit;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command_address = command_get_selected();
    casting_unit = unit_casting_get();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case TURN_COMMAND_PROMPT_YES:
        state_enter_unit_moving();
        return;
    case TURN_COMMAND_PROMPT_NO:
    case TURN_COMMAND_UNRESOLVED_FF:
        casting_unit->path.count = 0;
        state_enter_select_move_tile();
        return;
    }
}

/* UnitMoving game-state handler: advance the casting unit along its path. When
 * the last step has ended and no animation is pending, refresh its display and
 * open the move confirmation. */
void state_handle_unit_moving(void) {
    unit_runtime_t* casting_unit;

    camera_rotation_handle_input();
    camera_zoom_handle_input();
    camera_tilt_handle_input();
    casting_unit = unit_casting_get();
    move_step_update(casting_unit);
    if (casting_unit->step_phase == MOVE_STEP_PHASE_NONE
        && casting_unit->movement_path_offset >= casting_unit->path.count && g_state_animation_continue_check == 0) {
        unit_runtime_display_update(casting_unit->runtime_id);
        state_enter_move_confirm_menu();
    }
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
}

/* Put the unit back where it was before its move: restore its tile, facing, mount link and
 * standing-on id from the previous_* fields, re-register the tile in its battle stats and restart
 * its current animation. */
static void previous_state_restore(unit_runtime_t* unit) {
    unit->map_x = unit->previous_map_x;
    unit->map_y = unit->previous_map_y;
    unit->map_layer = unit->previous_map_layer;
    unit->facing = unit->previous_facing;
    unit->mount_state = unit->previous_mount_state;
    unit->mount_partner_runtime_id = unit->previous_mount_partner_runtime_id;
    unit->standing_on_battle_id_plus_one = unit->previous_standing_on_battle_id_plus_one;
    unit_set_move_and_world_coords(unit);
    unit_set_tile_position(
        unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_layer, (unit->facing / UNIT_FACING_QUARTER_TURN));
    animation_facing_force(unit->animation.sequence_id >> 1, unit->facing, unit);
}

/* Cancel a move at the confirm prompt. The unit returns to its previous tile;
 * a mount takes its rider back with it, a unit that was riding is put back on
 * its mount, and a mount it climbed onto during the move is freed. */
static void move_undo(unit_runtime_t* unit) {
    unit_runtime_t* other;

    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        other = unit_runtime_get(unit->mount_partner_runtime_id);
        other->mount_state = UNIT_MOUNT_STATE_NONE;
        unit_runtime_display_update(other->runtime_id);
    }
    previous_state_restore(unit);
    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        other = unit_runtime_get(unit->mount_partner_runtime_id);
        if (other != 0) {
            previous_state_restore(other);
            unit_runtime_display_update(other->runtime_id);
        }
    } else if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        unit->status_flags_1_4 |= UNIT_STATUS_MOUNTED;
        other = unit_runtime_get(unit->mount_partner_runtime_id);
        if (other != 0) {
            other->mount_state = UNIT_MOUNT_STATE_MOUNT;
            other->mount_partner_runtime_id = unit->runtime_id;
            unit_runtime_display_update(other->runtime_id);
        }
    } else {
        unit->status_flags_1_4 &= ~UNIT_STATUS_MOUNTED;
    }
    unit_runtime_display_update(unit->runtime_id);
}

/*
 * Keep the move chosen at the confirm prompt: the moved unit re-registers its
 * tile and, when it carries a rider (mount state MOUNT), the rider takes its
 * tile, destination, and real and world position raised 10 units (0xa000 in
 * Q12). Then the acting unit's move is marked taken and the post-movement
 * steps start.
 */
static void move_confirm(unit_runtime_t* unit, unit_runtime_t* moved) {
    unit_runtime_t* rider;

    if (moved->stats != 0) {
        unit_set_tile_position(moved->stats->battle_id, moved->map_x, moved->map_y, moved->map_layer,
            (moved->facing / UNIT_FACING_QUARTER_TURN));
        if (moved->mount_state == UNIT_MOUNT_STATE_MOUNT) {
            rider = unit_runtime_get(moved->mount_partner_runtime_id);
            if (rider != 0) {
                rider->map_x = rider->movement.destination_x = moved->map_x;
                rider->map_y = rider->movement.destination_y = moved->map_y;
                rider->map_layer = rider->movement.destination_layer = moved->map_layer;
                rider->real = moved->real;
                rider->world_coords = moved->world_coords;
                rider->world_coords.vy -= 10;
                rider->real.vy -= 0xa000;
                unit_set_tile_position(rider->stats->battle_id, moved->map_x, moved->map_y, moved->map_layer,
                    (moved->facing / UNIT_FACING_QUARTER_TURN));
            }
        }
    }
    unit->turn_action_flags |= TURN_ACTION_MOVED;
    turn_move_taken_set(unit->stats->battle_id);
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) && !(unit->path.flags & MOVE_EFFECTIVE_TELEPORT)) {
        sound_sfx_play(SOUND_SFX_CONFIRM);
    }
    sprite_palette_restore_all();
    move_start_post_events();
}

/* MoveConfirmMenu game-state handler. YES keeps the move and finishes it. NO or UNRESOLVED_FF
 * undoes it and returns to choosing a destination, except after a teleport, which is kept as with
 * YES. */
void state_handle_move_confirm_menu(void) {
    turn_command_t* selected_command_address;
    s32 selected_command;
    unit_runtime_t* source_unit;
    unit_runtime_t* casting_unit;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command_address = command_get_selected();
    source_unit = unit_source_get();
    casting_unit = unit_casting_get();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case TURN_COMMAND_PROMPT_NO:
    case TURN_COMMAND_UNRESOLVED_FF:
        if (casting_unit->path.flags & MOVE_EFFECTIVE_TELEPORT) {
        case TURN_COMMAND_PROMPT_YES:
            move_confirm(source_unit, casting_unit);
            return;
        }
        move_undo(casting_unit);
        sprite_palette_restore_all();
        state_enter_select_move_tile();
        return;
    }
}

/* WaitMenu game-state handler. YES goes on to choosing a facing. NO or UNRESOLVED_FF returns to the
 * after-command state, unless the unit's turn is over (turn_is_over returns
 * 1): then it chooses a facing too. */
void state_handle_wait_menu(void) {
    turn_command_t* selected_command_address;
    s32 selected_command;
    unit_runtime_t* source_unit;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command_address = command_get_selected();
    source_unit = unit_source_get();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case TURN_COMMAND_PROMPT_NO:
    case TURN_COMMAND_UNRESOLVED_FF:
        if (turn_is_over(source_unit->stats->battle_id) != 1) {
            state_enter_after_command();
            return;
        }
    case TURN_COMMAND_PROMPT_YES:
        state_enter_wait_direction();
        return;
    }
}

/* Write the unit's tile and facing back to its battle stats; a unit with a mount partner also
 * gives the partner its facing and the same tile. */
static void tile_position_with_partner_set(unit_runtime_t* unit) {
    unit_runtime_t* mounted_unit;

    unit_set_tile_position(
        unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_layer, (unit->facing / UNIT_FACING_QUARTER_TURN));

    if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
        mounted_unit = unit_runtime_get(unit->mount_partner_runtime_id);
        if (mounted_unit != 0) {
            mounted_unit->facing = unit->facing;
            mounted_unit->camera_facing_quadrant = unit->camera_facing_quadrant;
            mounted_unit->camera_facing_sixteenth = unit->camera_facing_sixteenth;

            unit_set_tile_position(mounted_unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_layer,
                (unit->facing / UNIT_FACING_QUARTER_TURN));
        }
    }
}

/* WaitDirection game-state handler: the acting unit picks the facing it ends its
 * turn with.
 *
 * A player-controlled unit shows the direction indicator and turns with the pad
 * (a mount or rider turns with it); circle ends the turn and runs the
 * between-turn events, cross returns to the after-command state. An AI-controlled
 * unit takes its stored facing hint (0-3, in quarter turns) and ends the turn
 * once its state frame counter reaches 0x1f.
 *
 * Ending the turn applies turn_end and re-registers the unit's tile
 * together with its mount partner. */
void state_handle_wait_direction(void) {
    s32 facing;
    s32 facing_hint;
    u16 frame;
    unit_runtime_t* unit = unit_source_get();
    unit_runtime_t* mounted_unit;

    sprite_units_project();
    if (g_menu_status_screen_selected != 1) {
        unit_animations_update();
    }
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        unit_facing_draw_wait_direction();
    }
    sprite_units_display_update();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        camera_rotation_handle_input();
        camera_zoom_handle_input();
        camera_tilt_handle_input();
        if ((g_input_pressed_buttons & PSX_PAD_CIRCLE) != 0) {
            if (unit->stats != 0) {
                turn_end(unit->stats->battle_id);
                tile_position_with_partner_set(unit);
            }
            sound_sfx_play(SOUND_SFX_CONFIRM);
            turn_next_event_start();
        } else if ((g_input_pressed_buttons & PSX_PAD_CROSS) != 0) {
            state_enter_after_command();
        }
        /* The target passes only the mode; the definition's second parameter is a matching device. */
        facing = cursor_input_direction_get(CAMERA_INPUT_PRESSED);
        if (facing != -1 && facing != g_state_wait_direction_facing) {
            sound_sfx_play(SOUND_SFX_CURSOR_MOVE);
            g_state_wait_direction_facing = facing;
        }
        unit_facing_set_cardinal(unit, facing);
        if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
            mounted_unit = unit_runtime_get(unit->mount_partner_runtime_id);
            if (mounted_unit != 0) {
                unit_facing_set_cardinal(mounted_unit, facing);
            }
        }
    } else {
        facing_hint = unit->command_state.cursor.facing_hint;
        if (facing_hint < 4) {
            unit_facing_set_cardinal(unit, facing_hint * UNIT_FACING_QUARTER_TURN);
            if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
                mounted_unit = unit_runtime_get(unit->mount_partner_runtime_id);
                if (mounted_unit != 0) {
                    unit_facing_set_cardinal(
                        mounted_unit, unit->command_state.cursor.facing_hint * UNIT_FACING_QUARTER_TURN);
                }
            }
        }
        frame = unit->state_frame_counter++;
        if (frame >= 0x1f) {
            if (unit->stats != 0) {
                turn_end(unit->stats->battle_id);
                tile_position_with_partner_set(unit);
            }
            turn_next_event_start();
        }
    }
}

/*
 * CrystalLearn game-state handler: apply the crystal or treasure choice
 * (0, LEARN_ABILITIES and TAKE_TREASURE pass their value to
 * crystal_choice_apply, RESTORE_HP_MP restores HP and MP and
 * finalizes the action, and the prompt answers only close). Then, once no effect is pending, the casting unit's
 * frame counter has reached 0x1f and no numeric display shows, work on the first
 * pending post-movement event of g_move_post_events, clearing it once
 * done, and start the next post-movement step.
 */
void state_handle_post_move_events(void) {
    turn_command_t* command;
    unit_runtime_t* source;
    unit_runtime_t* casting;
    u16 counter;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    command = command_get_selected();
    source = unit_source_get();
    casting = unit_casting_get();
    switch (command->kind) {
    case CRYSTAL_RESTORE_HP_MP:
        crystal_choice_apply(source->stats, CRYSTAL_RESTORE_HP_MP);
        g_state_window_closed = 1;
        action_apply_target_result(source->stats->battle_id);
        result_display_prepare(source);
        unit_runtime_display_update(source->runtime_id);
        break;
    case 0:
    case CRYSTAL_LEARN_ABILITIES:
    case CRYSTAL_TAKE_TREASURE:
        crystal_choice_apply(source->stats, command->kind);
        g_state_window_closed = 1;
        break;
    case TURN_COMMAND_PROMPT_YES:
    case TURN_COMMAND_PROMPT_NO:
    case TURN_COMMAND_UNRESOLVED_FF:
        g_state_window_closed = 1;
        break;
    }

    if (g_state_window_closed == 0) {
        return;
    }
    if (g_state_animation_continue_check != 0) {
        return;
    }
    counter = casting->state_frame_counter;
    casting->state_frame_counter = counter + 1;
    if (counter < 0x1f) {
        return;
    }
    if (source->numeric_display.active != 0) {
        return;
    }
    if (casting->numeric_display.active != 0) {
        return;
    }

    if (g_move_post_events & MOVE_POST_EVENT_CRYSTAL_OR_TREASURE) {
        if (unit_tile_pickup_find(source->map_x, source->map_y, source->map_layer) != 0) {
            return;
        }
        g_move_post_events &= ~MOVE_POST_EVENT_CRYSTAL_OR_TREASURE;
        ai_selected_action_init();
    } else if (g_move_post_events & MOVE_POST_EVENT_MOVEMENT_BENEFIT) {
        g_reward_report_phase += 1;
        if (g_reward_report_phase >= 2) {
            g_move_post_events &= ~MOVE_POST_EVENT_MOVEMENT_BENEFIT;
        }
    } else if (g_move_post_events & MOVE_POST_EVENT_ITEM_FOUND) {
        /* Wait out a non-idle animation still playing (the find reaction). */
        if ((source->animation.wait != 0) && ((source->animation.sequence_id >> 1) >= ANIMATION_WALK)) {
            return;
        }
        source->item_display_active = 0;
        g_move_post_events &= ~MOVE_POST_EVENT_ITEM_FOUND;
    } else if (g_move_post_events & MOVE_POST_EVENT_TRAP_TRIGGERED) {
        g_move_post_events &= ~MOVE_POST_EVENT_TRAP_TRIGGERED;
        ai_selected_action_init();
        unit_runtime_display_update(source->runtime_id);
    } else if (g_move_post_events & MOVE_POST_EVENT_CHARGING_CANCEL) {
        g_move_post_events &= ~MOVE_POST_EVENT_CHARGING_CANCEL;
    } else if (g_move_post_events & MOVE_POST_EVENT_SOURCE_DISPLAY) {
        g_move_post_events &= ~MOVE_POST_EVENT_SOURCE_DISPLAY;
    } else if (g_move_post_events & MOVE_POST_EVENT_MOUNT_STATUS_CHANGE) {
        g_move_post_events &= ~MOVE_POST_EVENT_MOUNT_STATUS_CHANGE;
    } else {
        return;
    }
    move_start_next_post_event();
}

/* TargetingPrompt game-state handler: YES goes on to the range display
 * (target_tile_selection_start); NO or UNRESOLVED_FF returns to the idle action menu. */
void state_handle_targeting_prompt(void) {
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    switch (selected_command) {
    case TURN_COMMAND_PROMPT_YES:
        target_tile_selection_start();
        return;
    case TURN_COMMAND_PROMPT_NO:
    case TURN_COMMAND_UNRESOLVED_FF:
        menu_action_open_active_unit();
        return;
    }
}

/* TargetingRange game-state handler: the ability's range is shown and its target tile chosen. A
 * player-controlled unit moves the cursor and camera; cross returns to the idle action menu,
 * circle selects the tile when it is in range and otherwise enters the illegal-range state. An AI
 * unit moves the cursor to its stored target tile or unit and selects it once its state frame
 * counter reaches 0x1f; a missing target unit reopens the action menus. */
void state_handle_targeting_range(void) {
    u16 frame;
    unit_runtime_t* unit = unit_source_get();
    unit_runtime_t* target;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        cursor_move_by_input();
        camera_rotation_handle_input();
        camera_zoom_handle_input();
        camera_tilt_handle_input();
        if ((g_input_pressed_buttons & PSX_PAD_CROSS) != 0) {
            target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 2);
            menu_action_open_active_unit();
            return;
        }
        if ((g_input_pressed_buttons & PSX_PAD_CIRCLE) != 0) {
            if ((battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer)
                        ->ceiling_depth_and_marks.ability_range)
                != 0) {
                target_tile_select();
                return;
            }
            state_enter_illegal_range();
        }
    } else {
        switch (unit->command_state.command.data.action.targeting_type) {
        case TARGET_TYPE_TILE:
            cursor_move_to_selection(unit, 0);
            frame = unit->state_frame_counter++;
            if (frame >= 0x1f) {
                /* This lookup and the one below discard their result. */
                battle_map_tile_get_pointer(unit->command_state.cursor.target_panel.vx,
                    unit->command_state.cursor.target_panel.vz, unit->command_state.cursor.target_panel.vy);
                target_tile_select();
            }
            return;
        case TARGET_TYPE_UNIT:
            target = unit_battle_get(unit->command_state.command.data.action.target_id);
            if (target != 0) {
                cursor_move_to_selection(unit, target);
                frame = unit->state_frame_counter++;
                if (frame >= 0x1f) {
                    battle_map_tile_get_pointer(target->map_x, target->map_y, target->map_layer);
                    target_tile_select();
                }
            } else {
                state_enter_open_action_menus();
            }
            break;
        }
    }
}

/* IllegalRange game-state handler: when the message closes (any answer), return to the range
 * display. */
void state_handle_illegal_range(void) {
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= TURN_COMMAND_PROMPT_YES
        && (selected_command <= TURN_COMMAND_PROMPT_NO || selected_command == TURN_COMMAND_UNRESOLVED_FF)) {
        target_tile_selection_start();
    }
}

/* AbilityPreviewHelp game-state handler: when the help window has gone
 * (menu_thread_activity_get returns something other than 2), return to the
 * ability preview with map panning. */
void state_handle_ability_preview_help(void) {
    if (menu_thread_activity_get() != 2) {
        g_state_in_help = 0;
        camera_pan_enable();
        g_game_state = GAME_STATE_ABILITY_PREVIEW_HANDLING;
    }
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
}

/* AbilityPreviewHandling game-state handler: the pending ability's targets are
 * marked and the unit confirms or backs out.
 *
 * For a player-controlled unit circle restores the cursor input mode and opens
 * the confirmation prompt (state_enter_confirm_action). Cross clears the target marks
 * and returns to the idle action menu when the ability has no target choice,
 * otherwise to the range display (target_tile_selection_start). An AI-controlled
 * unit confirms by itself once its state frame counter reaches 0x1f. */
void state_handle_ability_preview(void) {
    u16 frame;
    unit_runtime_t* unit = unit_source_get();

    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        camera_rotation_handle_input();
        camera_zoom_handle_input();
        camera_tilt_handle_input();
        if ((g_input_pressed_buttons & PSX_PAD_CIRCLE) != 0) {
            g_cursor_input_mode = g_cursor_input_mode_saved;
            sprite_palette_restore_all();
            state_enter_confirm_action();
        } else if ((g_input_pressed_buttons & PSX_PAD_CROSS) != 0) {
            g_cursor_input_mode = g_cursor_input_mode_saved;
            target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_HIDE_TARGETS, 3);
            if (unit->ability_preview_phase == TARGET_PANELS_NO_TARGETING) {
                target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 3);
                sprite_palette_restore_all();
                menu_action_open_active_unit();
            } else {
                target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 3);
                sprite_palette_restore_all();
                target_tile_selection_start();
            }
        }
    } else {
        frame = unit->state_frame_counter;
        unit->state_frame_counter = frame + 1;
        if (frame >= 0x1f) {
            g_cursor_input_mode = g_cursor_input_mode_saved;
            sprite_palette_restore_all();
            state_enter_confirm_action();
        }
    }
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
}

/* ConfirmAction game-state handler. The answers double as targeting types. For a player-controlled
 * unit, TARGET_TYPE_TILE aims the action at the cursor tile, and TARGET_TYPE_UNIT
 * or a plain YES at the unit on it (the tile when it is empty); the answer is stored as the
 * targeting type as it is, and both store the cursor tile and start the pre-attack animation. NO or
 * UNRESOLVED_FF clears the marks and returns to the range display. An AI unit starts the pre-attack
 * animation on TILE, UNIT or YES. */
void state_handle_confirm_action(void) {
    unit_runtime_t* unit;
    turn_command_t* selected_command_address;
    unit_runtime_t* target;
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command_address = command_get_selected();
    unit = unit_source_get();
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        selected_command = selected_command_address->kind;
        switch (selected_command) {
        case TARGET_TYPE_TILE:
            unit->command_state.command.data.action.targeting_type = selected_command_address->kind;
            vector_short_set(&unit->command_state.cursor.target_panel, g_cursor_x, g_cursor_layer, g_cursor_y);
            state_enter_pre_attack_animation();
            return;
        case TARGET_TYPE_UNIT:
        case TURN_COMMAND_PROMPT_YES:
            target = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
            if (target != 0) {
                unit->command_state.command.data.action.targeting_type = selected_command_address->kind;
                unit->command_state.command.data.action.target_id = target->stats->battle_id;
            } else {
                unit->command_state.command.data.action.targeting_type = TARGET_TYPE_TILE;
            }
            vector_short_set(&unit->command_state.cursor.target_panel, g_cursor_x, g_cursor_layer, g_cursor_y);
            state_enter_pre_attack_animation();
            return;
        case TURN_COMMAND_PROMPT_NO:
        case TURN_COMMAND_UNRESOLVED_FF:
            target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 3);
            target_tile_selection_start();
            return;
        default:
            break;
        }
    } else {
        selected_command = selected_command_address->kind;
        switch (selected_command) {
        case TARGET_TYPE_TILE:
        case TARGET_TYPE_UNIT:
        case TURN_COMMAND_PROMPT_YES:
            state_enter_pre_attack_animation();
            return;
        case TURN_COMMAND_PROMPT_NO:
        case TURN_COMMAND_UNRESOLVED_FF:
            target_tile_mark_colors_set(BATTLE_MAP_TILE_MARK_NONE, 3);
            target_tile_selection_start();
            return;
        default:
            break;
        }
    }
}

/* PreAttackAnimation game-state handler: execute the ability once the source
 * unit's animation has ended or looped, or its state frame counter has reached
 * 0x3d, and the camera has settled. */
void state_handle_pre_attack_animation(void) {
    unit_runtime_t* unit;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    unit = unit_source_get();
    if ((unit->animation.wait == 0 || unit->animation.restart_count != 0 || unit->state_frame_counter >= 0x3d)
        && (g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0
        && g_camera_rotation_action == CAMERA_ROTATION_NONE) {
        action_execute_ability();
    }
}

/* ActionCast game-state handler: once the message has closed and the camera has
 * settled, enter the commence-attack phase. */
void state_handle_action_cast(void) {
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    /* Any prompt answer marks the window closed (the same test as the
     * BATTLE_MESSAGE_DISPLAY handler). */
    if (selected_command >= TURN_COMMAND_PROMPT_YES
        && (selected_command <= TURN_COMMAND_PROMPT_NO || selected_command == TURN_COMMAND_UNRESOLVED_FF)) {
        g_state_window_closed = 1;
    }
    /* Wait for the camera to settle before handing the action data over. */
    if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_state_window_closed != 0) {
        state_enter_commence_attack_phase();
    }
}

/* CommenceAttackPhase game-state handler. A Jump waits for the source unit's
 * animation to end, then marks its action taken and enters the after-command
 * state. Any other action starts the pre-action phase and announces the next
 * action that is due. */
void state_handle_commence_attack_phase(void) {
    unit_runtime_t* unit;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    unit = unit_source_get();
    if (unit->attack_phase_state == ACTION_PREPARATION_JUMP) {
        if (unit->animation.wait == 0) {
            turn_act_taken_set(unit->stats->battle_id);
            action_end_command(unit);
        }
    } else {
        g_action_phase = ACTION_PHASE_PRE_ACTION;
        action_announce_next();
    }
}

/* ActionAnnouncement game-state handler: when the announcement message closes (any answer), enter
 * the OpenSp2Files state. */
void state_handle_action_announcement(void) {
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= TURN_COMMAND_PROMPT_YES
        && (selected_command <= TURN_COMMAND_PROMPT_NO || selected_command == TURN_COMMAND_UNRESOLVED_FF)) {
        state_enter_open_sp2_files();
    }
}

/* OpenSp2Files game-state handler: load the SP2 sheet holding the casting unit's
 * attack animation, then start the strike
 * (action_start_next_strike).
 *
 * With no buffer yet, a 0x8000-byte buffer is allocated and the read requested. A
 * unit that needs no SP2 file frees the buffer and starts the strike at once.
 * Retail also frees the buffer when the request was accepted (0) and keeps it
 * when the loader was busy (1): the next frame allocates again, that request is
 * refused, and the file arrives in the kept block only because the heap hands the
 * freed pages back. Once the file has loaded, the first free of the two VRAM
 * slots (owner 0xff) takes the unit's id | 0x40 as owner, the 64x256 image is
 * uploaded to the slot's rectangle and the strike starts. While neither slot is
 * free the state waits. */
void state_handle_open_sp2_files(void) {
    RECT image_rect;
    unit_runtime_t* unit = unit_casting_get();
    s32 open_result;
    s32 slot;
    s32 free_owner;
    s32 offset;
    u32* sp2_data;

    if (g_sprite_sp2_data == 0) {
        g_sprite_sp2_data = heap_alloc(0x8000);
        open_result = sprite_sp2_file_open(unit, g_sprite_sp2_data);
        if (open_result == -1) {
            heap_free(g_sprite_sp2_data);
            g_sprite_sp2_data = 0;
            action_start_next_strike();
        } else if (open_result == 0) {
            heap_free(g_sprite_sp2_data);
            g_sprite_sp2_data = 0;
        }
    } else if (file_load_state_get() == 0) {
        slot = 0;
        free_owner = SPRITE_EVTCHR_SLOT_OWNER_NONE;
        /* Keep the target's byte-offset induction; advancing a slot pointer changes the exact code. */
        offset = 0;
        do {
            if (((sprite_evtchr_slot_t*)((u8*)g_sprite_evtchr_slots + offset))->owner == free_owner) {
                sp2_data = g_sprite_sp2_data;
                ((sprite_evtchr_slot_t*)((u8*)g_sprite_evtchr_slots + offset))->owner
                    = unit->runtime_id | SPRITE_EVTCHR_SLOT_OWNER_SP2;
                image_rect.x = ((sprite_evtchr_slot_t*)((u8*)g_sprite_evtchr_slots + offset))->image_rect.x;
                image_rect.y = ((sprite_evtchr_slot_t*)((u8*)g_sprite_evtchr_slots + offset))->image_rect.y;
                image_rect.w = 0x40;
                image_rect.h = 0x100;
                LoadImage(&image_rect, sp2_data);
                action_start_next_strike();
                break;
            }
            slot++;
            offset += sizeof(sprite_evtchr_slot_t);
        } while (slot < SPRITE_EVTCHR_SLOT_COUNT);
    }
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
}

/* StartEffectFileOpen game-state handler: step the effect-file load
 * (effect_file_load_update); when no work remains, resolve the strike and pick
 * the attacker's animation. */
void state_handle_start_effect_file_open(void) {
    if (effect_file_load_update() != 0) {
        g_state_animation_continue_check = 1;
    } else {
        g_state_animation_continue_check = 0;
    }
    if (g_state_animation_continue_check == 0) {
        action_apply_strike_results();
    }
    state_update_units();
}

/* ActionExecuteStart game-state handler: count the frame and enter the
 * action-execute state (state_enter_action_execute). */
void state_handle_action_execute_start(void) {
    unit_runtime_t* unit = unit_casting_get();

    unit->state_frame_counter++;
    state_enter_action_execute();
    state_update_units();
}

/* Start the target's reaction animation for the attacker's attack and prepare its post-action
 * display; nothing happens when either unit is missing. */
static void target_animation_display_start(unit_runtime_t* attacker, unit_runtime_t* target) {
    if (attacker != 0 && target != 0) {
        action_start_target_response(attacker, target);
        result_runtime_display_prepare(target->runtime_id);
    }
}

/* ActionExecute game-state handler: animate the strike, run the knockback step of
 * every target and, when everything has settled, refresh the targets and show the
 * battle message.
 *
 * The strike has settled when no effect cycle or target step is running
 * (g_state_animation_continue_check), the casting unit shows no numeric
 * display, and its animation countdown is 0 or its animation id is below
 * ANIMATION_ATTACK_FIRST: only a teleport or attack animation is
 * waited out.
 *
 * An ability other than Knockback and Throw Stone follows the cursor tile with
 * the camera only when it has no effect file or is a Charge ability; on settling
 * it restarts the background sound and refreshes each target's display. For a
 * reaction ability the target instead starts the post-attack animation display,
 * and only when the ability has no effect file and its attack animation is not
 * the weapon's. A plain attack (id 0), Knockback or Throw Stone always follows
 * the cursor tile and runs the menu layer each frame; Knockback prepares each
 * target's post-action display before the refresh. */
void state_handle_action_execute(void) {
    unit_runtime_t* unit;
    unit_runtime_t* target;
    s32 i;

    sprite_units_project();
    if (g_menu_status_screen_selected != 1) {
        unit_animations_update();
        fade_tint_fade_update();
        camera_real_coords_update_animation();
        camera_screen_offset_update_animation();
        camera_zoom_update_animation();
        camera_rotation_update_animation();
    }
    sprite_units_display_update();
    fade_tint_overlay_draw();
    cursor_update();

    unit = unit_casting_get();
    /* The target reads the 16-bit value of g_system_vsync_interval. */
    unit->state_frame_counter += g_system_vsync_interval;
    if (unit->strike.target_count != 0) {
        for (i = 0; i < unit->strike.target_count; i++) {
            target = unit_battle_get(unit->strike.target_list[i]);
            if (target != 0) {
                move_knockback_update(target);
                g_state_animation_continue_check |= target->step_phase;
            }
        }
    }

    if (unit->used_ability_id != 0 && unit->used_ability_id != ABILITY_ID_KNOCKBACK
        && unit->used_ability_id != ABILITY_ID_BASIC_SKILL_THROW_STONE) {
        if (effect_ability_has_no_file(unit->used_ability_id) != 0
            || (unit->used_ability_id >= ABILITY_ID_CHARGE_FIRST && unit->used_ability_id < ABILITY_ID_MATH_FIRST)) {
            camera_cursor_focus_update();
        }
        if (g_state_animation_continue_check == 0 && unit->numeric_display.active == 0
            && (unit->animation.wait == 0 || (unit->animation.sequence_id >> 1) < ANIMATION_ATTACK_FIRST)) {
            sound_background_replay();
            if (unit->strike.target_count != 0) {
                for (i = 0; i < unit->strike.target_count; i++) {
                    target = unit_battle_get(unit->strike.target_list[i]);
                    if (target != 0) {
                        if (unit->used_ability_id >= ABILITY_ID_REACTION_FIRST
                            && unit->used_ability_id < ABILITY_ID_SUPPORT_FIRST) {
                            if (effect_ability_has_no_file(unit->used_ability_id) != 0
                                && g_ability_animation_data[unit->used_ability_id].attack_animation
                                    != ANIMATION_ATTACK_USE_WEAPON) {
                                target_animation_display_start(unit, target);
                            }
                        } else {
                            unit_runtime_display_update(target->runtime_id);
                        }
                    }
                }
            }
            state_enter_battle_message_display();
        }
    } else {
        camera_cursor_focus_update();
        menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
        if (g_state_animation_continue_check == 0 && unit->numeric_display.active == 0
            && (unit->animation.wait == 0 || (unit->animation.sequence_id >> 1) < ANIMATION_ATTACK_FIRST)) {
            if (unit->strike.target_count != 0) {
                for (i = 0; i < unit->strike.target_count; i++) {
                    target = unit_battle_get(unit->strike.target_list[i]);
                    if (target != 0) {
                        if (unit->used_ability_id == ABILITY_ID_KNOCKBACK) {
                            result_runtime_display_prepare(target->runtime_id);
                        }
                        unit_runtime_display_update(target->runtime_id);
                    }
                }
            }
            state_enter_battle_message_display();
        }
    }
}

/* BattleMessageDisplay game-state handler: when the message closes (any answer), queue and show the
 * casting unit's post-effect messages. */
void state_handle_battle_message_display(void) {
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= TURN_COMMAND_PROMPT_YES
        && (selected_command <= TURN_COMMAND_PROMPT_NO || selected_command == TURN_COMMAND_UNRESOLVED_FF)) {
        g_state_window_closed = 1;
    }
    if (g_state_window_closed != 0) {
        action_message_show_for_caster();
    }
}

/* ResumeAttackPhase game-state handler: show the remaining post-effect messages, then finish the
 * strike.
 *
 * Each closed message (any answer) lets action_message_show_next open the next queued
 * one. When none remains and the casting unit's numeric display is idle, a continued attack starts
 * its next strike. Otherwise the SP2 buffer and VRAM slot are released, the acting unit's data is
 * stored and the units return to their status animations. Outside the pre-action phase the action
 * phase then advances (up to the reacting-unit phase) and the next due action is announced; when
 * none is due, or in the pre-action phase, the JP and EXP rewards begin.
 *
 * The facing computed for the acting unit's own action is passed to
 * action_post_action_facing_stub, which is empty. */
void state_handle_resume_attack_phase(void) {
    s32 facing;
    unit_stats_t* stats;
    s32 selected_command;
    unit_runtime_t* unit;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= TURN_COMMAND_PROMPT_YES
        && (selected_command <= TURN_COMMAND_PROMPT_NO || selected_command == TURN_COMMAND_UNRESOLVED_FF)) {
        g_state_window_closed = 1;
    }
    unit = unit_casting_get();
    if (g_state_window_closed != 0 && action_message_show_next() == 0 && unit->numeric_display.active == 0) {
        if (unit->strike.continue_attack != 0) {
            unit->continue_attack_count += 1;
            action_start_next_strike();
            return;
        }
        if (g_sprite_sp2_data != 0) {
            heap_free(g_sprite_sp2_data);
            g_sprite_sp2_data = 0;
        }
        sprite_evtchr_invalidate_sp2(unit);
        if (g_turn_event_type == TURN_EVENT_UNIT_READY && g_action_phase == ACTION_PHASE_ACTING_UNIT) {
            /* Facing in quarter turns: a signed division by 0x400 spelled out, because retail
             * finishes it with a logical shift. */
            facing = unit->facing;
            if (facing < 0) {
                facing += 0x3ff;
            }
            facing = (u32)facing >> 10;
            stats = unit->stats;
            action_post_action_facing_stub(stats, facing & 0xff);
        }
        action_store_actor_data(unit->stats);
        animation_status_set_on_main_stack(unit);
        unit_target_displays_update(unit);
        unit_source_get(); /* result unused */
        if (g_action_phase != ACTION_PHASE_PRE_ACTION) {
            if (g_action_phase != ACTION_PHASE_REACTING_UNIT) {
                g_action_phase += 1;
            }
            if (action_announce_next() != 0) {
                return;
            }
        }
        reward_grant();
    }
}

/* LearnAbilityOnHit game-state handler: steps through the post-action reports (level up, job level
 * up, learn on hit).
 *
 * YES on the learn-on-hit report teaches the used ability to the unit found for it. Every answer
 * advances to the next report, and after the third the action is marked finished. From then on,
 * once no animation is pending and neither the casting unit nor any of its targets still shows a
 * numeric display, the source unit returns to its status animation and the after-command state
 * begins. */
void state_handle_post_action_reports(void) {
    turn_command_t* selected_command_address;
    s32 selected_command;
    s32 i;
    s32 display_active;
    s32 target_count;
    unit_runtime_t* casting_unit;
    unit_runtime_t* target_unit;
    u8 compiler_stack_pad[8];

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command_address = command_get_selected();
    casting_unit = unit_casting_get();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case TURN_COMMAND_PROMPT_YES:
        if (g_reward_report_phase == REWARD_REPORT_LEARN_ON_HIT) {
            target_unit = unit_battle_get(g_ability_learn_on_hit_unit_id);
            ability_learn_used(target_unit->stats);
        }
    case TURN_COMMAND_PROMPT_NO:
    case TURN_COMMAND_UNRESOLVED_FF:
        g_reward_report_phase += 1;
        if (g_reward_report_phase >= 3) {
            g_state_window_closed = 1;
        } else {
            state_enter_post_action_reports();
        }
        break;
    }
    if (g_state_window_closed != 0 && g_state_animation_continue_check == 0) {
        i = 0;
        if (casting_unit->numeric_display.active == 0) {
            target_count = casting_unit->strike.target_count;
            display_active = 0;
            if (target_count > 0) {
                do {
                    target_unit = unit_battle_get(casting_unit->strike.target_list[i]);
                    if (target_unit != 0 && target_unit->numeric_display.active != 0) {
                        display_active = 1;
                        break;
                    }
                    target_count = casting_unit->strike.target_count;
                    i += 1;
                } while (i < target_count);
            }
            if (display_active == 0) {
                casting_unit = unit_source_get();
                animation_status_set_on_main_stack(casting_unit);
                action_end_command(casting_unit);
            }
        }
    }
}

/* Frame step shared by the two Deep Dungeon mesh states: run the unit and menu update and, once
 * the map data has loaded, refresh every unit's display, clear
 * g_battle_map_deep_dungeon_load_in_progress and return 1. Returns 0 while the load continues. */
static s32 deep_dungeon_mesh_load_update(void) {
    s32 runtime_id;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    if (battle_map_load_poll() != 0) {
        return 0;
    }
    for (runtime_id = 0; runtime_id < UNIT_RUNTIME_SLOT_COUNT; runtime_id++) {
        unit_runtime_t* unit = unit_runtime_get(runtime_id);
        if (unit != 0) {
            unit_runtime_display_update(unit->runtime_id);
        }
    }
    g_battle_map_deep_dungeon_load_in_progress = 0;
    return 1;
}

/* DeepDungeonMeshLoad game-state handler: when the Deep Dungeon map has loaded,
 * run the between-turn events. */
void state_handle_deep_dungeon_mesh_load(void) {
    if (deep_dungeon_mesh_load_update() != 0) {
        turn_next_event_start();
    }
}

/* DeepDungeonMeshFinish game-state handler: when the Deep Dungeon map has loaded,
 * enter the after-command state. */
void state_handle_deep_dungeon_mesh_finish(void) {
    if (deep_dungeon_mesh_load_update() != 0) {
        state_enter_after_command();
    }
}

/* TargetSelectStart game-state handler: when the prompt closes (any answer), enter target
 * selection. */
void state_handle_target_select_start(void) {
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= TURN_COMMAND_PROMPT_YES
        && (selected_command <= TURN_COMMAND_PROMPT_NO || selected_command == TURN_COMMAND_UNRESOLVED_FF)) {
        state_enter_target_select();
    }
}

/* TargetSelect game-state handler: the player picks the auto-battle target with the cursor. Circle
 * confirms the unit on the tile when it suits the choice (an enemy for the targeted choice, an
 * ally for the recovery choice) and denies it otherwise or on an empty tile; cross moves the
 * cursor back to the casting unit and reopens the action menus, and triangle only moves the cursor
 * back. */
void state_handle_target_select(void) {
    unit_runtime_t* caster;
    unit_runtime_t* target;

    camera_rotation_handle_input();
    camera_zoom_handle_input();
    camera_tilt_handle_input();
    cursor_move_by_input();
    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    if (g_input_pressed_buttons & PSX_PAD_CIRCLE) {
        caster = unit_casting_get();
        target = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
        /* The targeted choice requires an enemy, the recovery choice an ally. */
        if (target == 0) {
            state_enter_target_select_denied();
        } else if (caster->target_select_command == TURN_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT) {
            if ((caster->stats->team_flags & UNIT_TEAM_MASK) == (target->stats->team_flags & UNIT_TEAM_MASK)) {
                state_enter_target_select_denied();
            } else {
                state_enter_target_select_confirm();
            }
        } else if (caster->target_select_command == TURN_COMMAND_AUTOBATTLE_PROTECT_ALLIES_SELECT) {
            if ((caster->stats->team_flags & UNIT_TEAM_MASK) == (target->stats->team_flags & UNIT_TEAM_MASK)) {
                state_enter_target_select_confirm();
            } else {
                state_enter_target_select_denied();
            }
        }
        sprite_palette_restore_all();
    } else if (g_input_pressed_buttons & PSX_PAD_CROSS) {
        cursor_move_to_unit(unit_casting_get());
        sprite_palette_restore_all();
        state_enter_open_action_menus();
    } else if (g_input_pressed_buttons & PSX_PAD_TRIANGLE) {
        cursor_move_to_unit(unit_casting_get());
    }
}

/* TargetSelectDenied game-state handler: when the message closes (any answer), return to target
 * selection. */
void state_handle_target_select_denied(void) {
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= TURN_COMMAND_PROMPT_YES
        && (selected_command <= TURN_COMMAND_PROMPT_NO || selected_command == TURN_COMMAND_UNRESOLVED_FF)) {
        state_enter_target_select();
    }
}

/* TargetSelectConfirm game-state handler: YES stores the unit under the cursor as the casting
 * unit's auto-battle target and opens the next action menus; NO or UNRESOLVED_FF returns to target
 * selection. */
void state_handle_target_select_confirm(void) {
    unit_runtime_t* caster;
    unit_runtime_t* target;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    switch (command_get_selected()->kind) {
    case TURN_COMMAND_PROMPT_YES:
        caster = unit_casting_get();
        target = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
        auto_battle_store_target(target->stats->battle_id, caster->stats->battle_id);
        cursor_move_to_unit(caster);
        state_enter_open_action_menus();
        break;
    case TURN_COMMAND_PROMPT_NO:
    case TURN_COMMAND_UNRESOLVED_FF:
        state_enter_target_select();
        break;
    }
}

/* TargetDisplayStart game-state handler: once the prompt has closed (any answer) and the camera has
 * settled, enter the target display. */
void state_handle_target_display_start(void) {
    s32 selected_command;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= TURN_COMMAND_PROMPT_YES
        && (selected_command <= TURN_COMMAND_PROMPT_NO || selected_command == TURN_COMMAND_UNRESOLVED_FF)) {
        g_state_window_closed = 1;
    }
    if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_state_window_closed != 0) {
        state_enter_target_display();
    }
}

/* TargetDisplay game-state handler: circle or cross puts the cursor back on the
 * casting unit and opens the next action menus. */
void state_handle_target_display(void) {
    s32 buttons;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    buttons = g_input_pressed_buttons;
    if ((buttons & PSX_PAD_CIRCLE) || (buttons & PSX_PAD_CROSS)) {
        cursor_move_to_unit(unit_casting_get());
        state_enter_open_action_menus();
    }
}

/* AfterCommand game-state handler: once the window has closed (any answer) and the source unit's
 * numeric display is idle, enter the continue-turn state. */
void state_handle_after_command(void) {
    turn_command_t* command_address;
    s32 command;
    unit_runtime_t* unit;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    command_address = command_get_selected();
    unit = unit_source_get();
    command = command_address->kind;
    if (command >= TURN_COMMAND_PROMPT_YES
        && (command <= TURN_COMMAND_PROMPT_NO || command == TURN_COMMAND_UNRESOLVED_FF)) {
        g_state_window_closed = 1;
    }
    if ((g_state_window_closed != 0) && (unit->numeric_display.active == 0)) {
        state_enter_continue_turn();
    }
}

/* ContinueTurn game-state handler: move on once the camera has settled (no cursor
 * vector, no rotation).
 *
 * After a charged ability (turn event ABILITY_READY) a queued scenario event with
 * a nonzero finish operation starts; otherwise the between-turn events run. On a
 * unit's own turn, a unit whose turn is over (turn_is_over
 * returns 1) gets its last command and ability copied back into its runtime record;
 * a Jump then resets its preparation and goes to the between-turn events, and
 * anything else opens the Wait menu. A unit whose turn goes on gets its action
 * menus back. */
void state_handle_continue_turn(void) {
    unit_runtime_t* unit;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    unit = unit_source_get();
    if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) != 0) {
        return;
    }
    if (g_camera_rotation_action != CAMERA_ROTATION_NONE) {
        return;
    }
    if (g_turn_event_type == TURN_EVENT_ABILITY_READY) {
        if (menu_request_dispatch(MENU_REQUEST_SCENARIO_CHECK, MENU_SCENARIO_CHECK_TEST, unit->stats->battle_id, 0, 1)
                == MENU_REQUEST_RESULT_EVENT
            && script_event_finish_operation_get() != 0) {
            g_game_state_previous = g_game_state;
            menu_request_dispatch(MENU_REQUEST_SCENARIO_CHECK, MENU_SCENARIO_CHECK_START, unit->stats->battle_id, 0, 1);
            state_enter_event();
            return;
        }
        turn_next_event_start();
        return;
    }
    if (turn_is_over(unit->stats->battle_id) == 1) {
        /* The stats keep a copy of the command the unit acted on. */
        unit->command_state.command.data.action = unit->stats->action_command;
        unit->used_ability_id = unit->stats->action_command.ability_id;
        if (unit->attack_phase_state == ACTION_PREPARATION_JUMP) {
            unit->attack_phase_state = ACTION_PREPARATION_CHARGE;
            unit_runtime_hide(unit->runtime_id);
            turn_next_event_start();
            return;
        }
        state_enter_wait_menu();
        return;
    }
    state_enter_open_action_menus();
}

/* ChangeTurn game-state handler. Once the menu layer reports an answer (the message closed) and the
 * camera has settled (no cursor vector, no rotation), the turn event type picks the next state: a
 * ready ability enters the action cast, an action result or a Mime action enters status execution,
 * and anything else opens the next action menus. */
void state_handle_change_turn(void) {
    s32 command;
    s32 type;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    command = command_get_selected()->kind;
    if (command >= TURN_COMMAND_PROMPT_YES) {
        if (command <= TURN_COMMAND_PROMPT_NO) {
            g_state_window_closed = 1;
        } else if (command == TURN_COMMAND_UNRESOLVED_FF) {
            g_state_window_closed = 1;
        }
    }
    if (g_state_window_closed != 0) {
        if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0) {
            if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
                unit_source_get(); /* result unused */
                type = g_turn_event_type;
                if (type == TURN_EVENT_ABILITY_READY) {
                    state_enter_action_cast();
                    return;
                }
                if ((type == TURN_EVENT_ACTION_RESULT) || (type == TURN_EVENT_MIME)) {
                    state_enter_status_execute();
                    return;
                }
                state_enter_open_action_menus();
            }
        }
    }
}

/* StatusExecute game-state handler: once the message has closed (any answer), no animation is
 * pending and the source unit's numeric display is idle, refresh the unit's display and run the
 * between-turn events. */
void state_handle_status_execute(void) {
    s32 selected_command;
    unit_runtime_t* unit;

    state_update_units();
    menu_event_frame_update(gfx_battle_otag_get(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= TURN_COMMAND_PROMPT_YES
        && (selected_command <= TURN_COMMAND_PROMPT_NO || selected_command == TURN_COMMAND_UNRESOLVED_FF)) {
        g_state_window_closed = 1;
    }
    if (g_state_window_closed != 0) {
        if (g_state_animation_continue_check == 0) {
            unit = unit_source_get();
            if (unit->numeric_display.active == 0) {
                unit_runtime_display_update(unit->runtime_id);
                turn_next_event_start();
            }
        }
    }
}

/* Effect game-state handler: animate the units, the screen color modulation and
 * the camera while an effect plays. When the effect cycle has ended, the game
 * state and vsync interval saved by state_enter_effect_playback are
 * restored. Neither function that stores this state has a caller, so the
 * handler does not run. */
void state_handle_effect(void) {
    sprite_units_project();
    if (g_menu_status_screen_selected != 1) {
        unit_animations_update();
        fade_tint_fade_update();
        camera_real_coords_update_animation();
        camera_screen_offset_update_animation();
        camera_zoom_update_animation();
        camera_rotation_update_animation();
    }
    sprite_units_display_update();
    fade_tint_overlay_draw();
    if (g_state_animation_continue_check == 0) {
        g_game_state = g_game_state_previous;
        g_system_vsync_interval = g_system_saved_vsync_interval;
    }
}

/* Event game-state frame: animate the units, screen color, camera and unit paths
 * (unless the status screen owns the frame), run one frame of the event script and
 * act on its finish result, then draw the screen fade while one is set. Result 0
 * returns to the battle through the between-turn events; 9 leaves for the world
 * map, 10 goes on to the next event in battle, 11 resets the game and 19 runs the
 * ending, each through CloseBattle.
 *
 * Keeping the loaded intensity and the clamped display level as separate
 * values retains the target's two moves before its unsigned 0x100 comparison.
 * The otherwise-unused 24-byte local preserves the original 64-byte frame. */
void state_handle_event(void) {
    s16 intensity;
    u16 level;
    char unused[24];

    sprite_units_project();
    if (g_menu_status_screen_selected != 1) {
        unit_animations_update();
        fade_tint_fade_update();
        camera_real_coords_update_animation();
        camera_screen_offset_update_animation();
        camera_zoom_update_animation();
        camera_rotation_update_animation();
        move_units_update();
    }
    sprite_units_display_update();
    fade_tint_overlay_draw();
    switch (script_event_frame_run(gfx_battle_otag_get(), g_input_current_buttons)) {
    case SCRIPT_EVENT_FRAME_NO_FINISH:
        sprite_evtchr_init_slots();
        g_menu_status_enabled = 1;
        ai_workspace_init();
        turn_next_event_start();
        break;
    case SCRIPT_EVENT_FRAME_TO_WORLD:
        sprite_evtchr_init_slots();
        state_enter_close_battle(8, SYSTEM_GAME_FLOW_WORLD);
        break;
    case SCRIPT_EVENT_FRAME_NEXT_EVENT:
        sprite_evtchr_init_slots();
        state_enter_close_battle(8, SYSTEM_GAME_FLOW_BATTLE);
        break;
    case SCRIPT_EVENT_FRAME_RESET:
        sprite_evtchr_init_slots();
        state_enter_close_battle(8, SYSTEM_GAME_FLOW_RESET);
        break;
    case SCRIPT_EVENT_FRAME_ENDING:
        sprite_evtchr_init_slots();
        state_enter_close_battle(8, SYSTEM_GAME_FLOW_ENDING);
        break;
    }

    if (g_fade_intensity != 0) {
        intensity = g_fade_intensity;
        level = intensity;
        if (level >= 0x100) {
            level = 0xff;
        }
        g_fade_polygons[g_gfx_screen_polarity].r0 = level;
        g_fade_polygons[g_gfx_screen_polarity].g0 = level;
        g_fade_polygons[g_gfx_screen_polarity].b0 = level;

        AddPrim(gfx_battle_otag_get(), &g_fade_polygons[g_gfx_screen_polarity]);
        AddPrim(gfx_battle_otag_get(), &g_fade_draw_modes[g_gfx_screen_polarity]);
    }
}

/* Stop the map's palette cycles and weather sound; every path that leaves a map calls it just
 * before unit_all_clear. */
void battle_map_animations_stop(void) {
    battle_map_stop_clut_cycles_and_weather_sfx();
}

/* MapJumpingOut game-state handler: darken the screen by
 * g_fade_step per frame. Past full black it stops the map
 * animations, clears the units and starts the map init sequence (MapInitialize),
 * which fades back in by itself. */
void state_handle_map_jumping_out(void) {
    u16 intensity;
    char unused[24];

    state_update_units();
    intensity = g_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }

    g_fade_polygons[g_gfx_screen_polarity].r0 = intensity;
    g_fade_polygons[g_gfx_screen_polarity].g0 = intensity;
    g_fade_polygons[g_gfx_screen_polarity].b0 = intensity;

    AddPrim(gfx_battle_otag_get(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_battle_otag_get(), &g_fade_draw_modes[g_gfx_screen_polarity]);

    g_fade_intensity += g_fade_step;
    if (g_fade_intensity >= 0x102) {
        g_fade_intensity = 0xff;
        g_game_state = GAME_STATE_MAP_INITIALIZE;
        g_state_map_init_step = 0;
        battle_map_animations_stop();
        unit_all_clear();
    }
}

/* MapJumpingOut2 game-state handler: darken the screen by
 * g_fade_step per frame. Past full black it stops the map
 * animations, clears the units and starts the map init sequence (MapInitialize2). */
void state_handle_change_map_jumping_out(void) {
    u16 intensity;
    char unused[24];

    state_update_units();
    intensity = g_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }

    g_fade_polygons[g_gfx_screen_polarity].r0 = intensity;
    g_fade_polygons[g_gfx_screen_polarity].g0 = intensity;
    g_fade_polygons[g_gfx_screen_polarity].b0 = intensity;

    AddPrim(gfx_battle_otag_get(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_battle_otag_get(), &g_fade_draw_modes[g_gfx_screen_polarity]);

    g_fade_intensity += g_fade_step;
    if (g_fade_intensity >= 0x102) {
        g_fade_intensity = 0xff;
        g_game_state = GAME_STATE_MAP_INITIALIZE_2;
        g_state_map_init_step = 0;
        battle_map_animations_stop();
        unit_all_clear();
    }
}

/* CloseBattle game-state handler: draw the fade overlay and raise its intensity
 * by g_fade_step. The game loop leaves in the frame it finds
 * this state, so the handler runs for at most one frame. */
void state_handle_close_battle(void) {
    u16 intensity;
    /* The original frame reserves this otherwise-unused stack area. */
    u8 stack_padding[0x18];

    state_update_units();
    intensity = g_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }
    g_fade_polygons[g_gfx_screen_polarity].r0 = intensity;
    g_fade_polygons[g_gfx_screen_polarity].g0 = intensity;
    g_fade_polygons[g_gfx_screen_polarity].b0 = intensity;
    AddPrim(gfx_battle_otag_get(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_battle_otag_get(), &g_fade_draw_modes[g_gfx_screen_polarity]);
    g_fade_intensity += g_fade_step;
}

/* MapInitialize game-state handler: run one step of the map init sequence behind
 * a black screen; at step 0xd start the fade-in (MapJumpingIn). */
void state_handle_map_init(void) {
    char unused[24]; /* unreferenced; sizes the target's 0x30-byte frame */

    g_state_map_init_step = battle_map_init_update(g_battle_map_id, g_state_map_init_step);
    g_fade_polygons[g_gfx_screen_polarity].r0 = 0xff;
    g_fade_polygons[g_gfx_screen_polarity].g0 = 0xff;
    g_fade_polygons[g_gfx_screen_polarity].b0 = 0xff;
    AddPrim(gfx_battle_otag_get(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_battle_otag_get(), &g_fade_draw_modes[g_gfx_screen_polarity]);
    if (g_state_map_init_step == 0xd) {
        g_game_state = GAME_STATE_MAP_JUMPING_IN;
        g_fade_intensity = 0xff;
    }
}

/* MapInitialize2 game-state handler, the map change an event asks for: run one
 * step of the map init sequence behind a black screen. At step 0xd the saved game
 * state resumes with the screen still black; the event fades it in with
 * state_start_change_map_jump_in. */
void state_handle_change_map_init(void) {
    s32 old_state;
    char unused[24]; /* unreferenced; sizes the target's 0x30-byte frame */

    g_state_map_init_step = battle_map_init_update(g_battle_map_id, g_state_map_init_step);
    g_fade_polygons[g_gfx_screen_polarity].r0 = 0xff;
    g_fade_polygons[g_gfx_screen_polarity].g0 = 0xff;
    g_fade_polygons[g_gfx_screen_polarity].b0 = 0xff;
    AddPrim(gfx_battle_otag_get(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_battle_otag_get(), &g_fade_draw_modes[g_gfx_screen_polarity]);
    if (g_state_map_init_step == 0xd) {
        old_state = g_game_state_previous;
        g_game_state_previous = GAME_STATE_FREE_CURSOR;
        g_fade_intensity = 0xff;
        g_game_state = old_state;
    }
}

/* MapJumpingIn game-state handler: lighten the screen by
 * g_fade_step per frame. When it is clear, the saved game
 * state resumes at vsync interval 1. */
void state_handle_map_jumping_in(void) {
    u16 intensity;
    s32 previous_state;
    char unused[24];

    state_update_units();
    intensity = g_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }
    g_fade_polygons[g_gfx_screen_polarity].r0 = intensity;
    g_fade_polygons[g_gfx_screen_polarity].g0 = intensity;
    g_fade_polygons[g_gfx_screen_polarity].b0 = intensity;
    AddPrim(gfx_battle_otag_get(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_battle_otag_get(), &g_fade_draw_modes[g_gfx_screen_polarity]);
    g_fade_intensity -= g_fade_step;
    if (g_fade_intensity <= 0) {
        previous_state = g_game_state_previous;
        g_fade_intensity = 0;
        g_game_state_previous = GAME_STATE_FREE_CURSOR;
        g_system_vsync_interval = 1;
        g_game_state = previous_state;
    }
}

/* MapJumpingIn2 game-state handler, the fade-in that ends an event's map change:
 * it advances the simulation unless the status screen owns the frame, runs the
 * event script and then draws the subtractive fade overlay one step lighter. When
 * the fade reaches zero the saved game state is restored. */
void state_handle_change_map_jumping_in(void) {
    u16 intensity;
    char unused[24];

    sprite_units_project();
    if (g_menu_status_screen_selected != 1) {
        unit_animations_update();
        fade_tint_fade_update();
        camera_real_coords_update_animation();
        camera_screen_offset_update_animation();
        camera_zoom_update_animation();
        camera_rotation_update_animation();
        move_units_update();
    }
    sprite_units_display_update();
    fade_tint_overlay_draw();
    /* TO_WORLD and NEXT_EVENT lead the other way round from state_handle_event
     * (QUIRKS.md). */
    switch (script_event_frame_run(gfx_battle_otag_get(), g_input_current_buttons)) {
    case SCRIPT_EVENT_FRAME_RUNNING:
        break;
    case SCRIPT_EVENT_FRAME_NO_FINISH:
        turn_next_event_start();
        break;
    case SCRIPT_EVENT_FRAME_TO_WORLD:
        state_enter_close_battle(8, SYSTEM_GAME_FLOW_BATTLE);
        break;
    case SCRIPT_EVENT_FRAME_NEXT_EVENT:
        state_enter_close_battle(8, SYSTEM_GAME_FLOW_WORLD);
        break;
    }

    intensity = g_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }
    g_fade_polygons[g_gfx_screen_polarity].r0 = intensity;
    g_fade_polygons[g_gfx_screen_polarity].g0 = intensity;
    g_fade_polygons[g_gfx_screen_polarity].b0 = intensity;

    AddPrim(gfx_battle_otag_get(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_battle_otag_get(), &g_fade_draw_modes[g_gfx_screen_polarity]);

    g_fade_intensity -= g_fade_step;
    if (g_fade_intensity <= 0) {
        g_fade_intensity = 0;
        g_game_state = g_game_state_previous;
        g_game_state_previous = GAME_STATE_FREE_CURSOR;
    }
}
