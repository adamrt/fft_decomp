#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_target.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Per-frame cursor update: project the cursor tile to the screen, and while
 * g_action_at_list_active is set also draw the selection cursor and the
 * tile glow. */
void target_calculate_tile_coords_and_glow_from_at_list(void) {
    SVECTOR secondary;
    SVECTOR tertiary;
    VECTOR primary;

    if (g_action_at_list_active != 0) {
        target_calculate_tile_coords_with_cursor_glow();
        return;
    }
    target_project_cursor_tile_to_screen(&primary, &secondary, &tertiary);
}

/* Per-frame unit update shared by most game-state handlers; it reads no input.
 *
 * Projects the units, animates them unless the status screen owns the frame,
 * updates the status bubbles and unit graphics, and projects the cursor tile,
 * drawing the map cursor and tile glow while g_action_at_list_active is
 * set. */
void battle_state_handle_free_cursor_input(void) {
    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    target_calculate_tile_coords_and_glow_from_at_list();
}

/* Free-cursor game state: camera and cursor input, then the button actions
 * (help, team highlight, act menu, move range, AT list and mini menu). */
void battle_state_handle_default_state(void) {
    battle_unit_t* unit;
    s32 input;
    RECT rect; /* never used; retail reserves its 8 bytes */

    camera_handle_rotation_input();
    camera_update_zoom_input();
    camera_update_tilt_input();
    target_move_cursor_by_input();
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    input = g_main_input_pressed_buttons;
    if (input & PSX_PAD_SELECT) {
        action_clear_at_list_id();
        battle_menu_open_free_cursor_help();
    } else if (input & PSX_PAD_SQUARE) {
        action_clear_at_list_id();
        battle_state_enter_highlight_units_by_team();
    } else if (input & PSX_PAD_CIRCLE) {
        target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 0);
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        if (battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z) != 0) {
            battle_menu_dispatch_idle_action_menu();
        } else {
            unit = battle_unit_get_source();
            if (unit != 0) {
                target_move_cursor_to_unit(unit);
                battle_menu_set_next_script_action_menus();
            }
        }
    } else if (input & PSX_PAD_CROSS) {
        action_clear_at_list_id();
        unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        if (unit != 0) {
            g_battle_casting_runtime_id = unit->runtime_id;
            target_show_move_range_in_free_cursor();
        }
    } else if (input & PSX_PAD_START) {
        if (g_main_game_options.fields.multi_height_cursor_speed != GAME_MULTI_HEIGHT_CURSOR_SPEED_STOP) {
            main_sound_play_sfx(MAIN_SFX_CONFIRM);
            action_show_next_at_list_entry();
            target_store_cursor_unit_name_and_data();
        }
    } else if (input & PSX_PAD_TRIANGLE) {
        action_clear_at_list_id();
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        battle_menu_open_free_cursor_mini_menu();
    }
}

/* FreeCursorHelp game-state handler: when the help window has gone, return to the
 * free cursor. */
void battle_state_handle_free_cursor_help_state(void) {
    if (menu_get_thread_activity_variant() != 2) {
        g_battle_menu_help_opening = 0;
        battle_state_set_free_cursor();
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* HighlightUnits game-state handler: releasing square restores the cursor input
 * mode and the unit palettes and returns to the free cursor. */
void battle_state_handle_highlight_units_state(void) {
    if (g_main_input_released_buttons & PSX_PAD_SQUARE) {
        g_battle_cursor_input_mode = g_battle_cursor_input_mode_saved;
        battle_state_set_free_cursor();
        battle_gfx_update_unit_palettes();
    }
    camera_handle_rotation_input();
    camera_update_zoom_input();
    camera_update_tilt_input();
    battle_state_handle_free_cursor_input();
}

/* OpenActionMenus game-state handler: once the window has closed (command 7, 8 or
 * 0xff) and the camera has settled, open the idle action menu of the unit under
 * the cursor. */
void battle_state_handle_open_action_menus_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        g_action_post_action = 1;
    }
    if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_action_post_action != 0) {
        battle_menu_dispatch_idle_action_menu();
    }
}

/* IdlingActionMenus game-state handler: act on the selected menu command.
 *
 * Move (0) and the 0x19 variant hand off to the move command, Act (1)
 * dispatches Defend, Equip Change and ordinary abilities on the command's
 * skillset, Wait (2) chooses a facing, and the remaining commands enter
 * target selection/display, the event check or scripted menus.
 *
 * The kind is switched as s32: the unsigned enum type makes GCC emit sltiu
 * where the target has slti. The empty cases for the unresolved commands 0xfe
 * and 0xff keep the switch a compare tree with the target's split: a
 * two-value range above 0x19 disables the jump table (range > 10 * count). */
void battle_state_handle_idling_action_menus_state(void) {
    ai_command_t* command;
    battle_unit_t* unit;
    s32 result;

    if (menu_get_thread_activity_variant() == 0) {
        battle_state_set_free_cursor();
    }
    if (g_main_input_pressed_buttons & PSX_PAD_SELECT) {
        battle_menu_open_help();
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    command = menu_get_selected_command_address();
    unit = battle_unit_get_casting();
    switch ((s32)command->kind) {
    case AI_COMMAND_MOVE:
        if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
            g_battle_casting_runtime_id = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id)->runtime_id;
        } else {
            g_battle_casting_runtime_id = g_battle_unit_source_runtime_id;
        }
        action_clear_at_list_id();
        action_handle_move_command();
        break;
    case AI_COMMAND_MOVE_SELF:
        g_battle_casting_runtime_id = g_battle_unit_source_runtime_id;
        action_clear_at_list_id();
        action_handle_move_command();
        break;
    case AI_COMMAND_ACT:
        action_clear_at_list_id();
        switch (command->data.action.skillset_id) {
        case SKILLSET_ID_DEFEND:
            unit->command_state.ai = *command;
            status_inflict_defending_to_battle_id(unit->stats->battle_id);
            unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            action_set_only_action_taken(unit->stats->battle_id);
            battle_state_enter_after_command();
            break;
        case SKILLSET_ID_EQUIP_CHANGE:
            unit->command_state.ai = *command;
            unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
            status_enable_disable_acting(unit->stats);
            ai_init_unit_abilities(unit->stats->battle_id);
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            action_set_only_action_taken(unit->stats->battle_id);
        case SKILLSET_ID_BLANK_04:
        case SKILLSET_ID_MIMIC:
            battle_state_enter_after_command();
            break;
        case SKILLSET_ID_ATTACK:
        default:
            unit->command_state.ai = *command;
            unit->command_state.ai.data.action.actor_id = unit->stats->battle_id;
            unit->command_state.ai.data.action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
            action_call_attack_preparation(&unit->command_state.ai.data.action);
            unit->used_ability_id = unit->command_state.ai.data.action.ability_id;
            result = target_set_panels_for_action(&unit->command_state.ai.data.action);
            unit->ability_preview_phase = result;
            switch (result) {
            case TARGET_PANELS_UNIT_OR_TILE:
            case TARGET_PANELS_TILE_ONLY:
            case TARGET_PANELS_NONE:
                battle_state_enter_action_execution_setup();
                break;
            case TARGET_PANELS_NO_TARGETING:
            default:
                target_select_tile();
                break;
            }
            break;
        }
        break;
    case AI_COMMAND_END_TURN:
        action_choose_wait();
        break;
    case AI_COMMAND_AUTOBATTLE_TARGETED_SELECT:
    case AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT:
        unit->target_select_command = command->kind;
        battle_state_enter_target_select_start();
        break;
    case AI_COMMAND_AUTOBATTLE_TARGETED_DISPLAY:
    case AI_COMMAND_AUTOBATTLE_RECOVERY_DISPLAY:
        unit->target_select_command = command->kind;
        battle_state_enter_target_display_start();
        break;
    case AI_COMMAND_AUTOBATTLE_FADING_LIFE:
    case AI_COMMAND_AUTOBATTLE_RETREAT:
        battle_menu_set_next_script_action_menus();
        break;
    case AI_COMMAND_NO_MENU_RECORD:
        battle_menu_clear_status_menu_state();
        action_check_between_turn_events();
        break;
    case AI_COMMAND_REFRESH_CONTROL:
        battle_unit_store_entd_flags_without_control_flag(unit);
        break;
    case AI_COMMAND_UNRESOLVED_FE:
    case AI_COMMAND_UNRESOLVED_FF:
        break;
    }
}

/*
 * Advance menu-to-targeting state and consume the unit's renderer command.
 *
 * AI setup may suspend before the command is ready. Keep each special
 * skillset's completion explicit so its distinct call tail is preserved.
 */
void battle_state_handle_menu_to_targeting_state(void) {
    battle_unit_t* unit;
    s32 kind;
    s32 result;
    u16 frame;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    unit = battle_unit_get_source();
    if (unit->state_frame_counter >= 31 && !unit->command_ready) {
        g_main_system_animation_speed = 2;
        if (!ai_build_command(unit->stats->battle_id, &unit->command_state.ai)) {
            if (unit->command_state.ai.kind == AI_COMMAND_ACT) {
                unit->command_state.ai.data.action.actor_id = unit->stats->battle_id;
                action_call_attack_preparation(&unit->command_state.ai.data.action);
            }
            unit->command_ready = 1;
            g_main_system_animation_speed = 1;
        }
    }
    frame = unit->state_frame_counter++;
    if (frame < 31 || !unit->command_ready)
        return;
    kind = unit->command_state.ai.kind;
    switch (kind) {
    case AI_COMMAND_MOVE:
        if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
            g_battle_casting_runtime_id = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id)->runtime_id;
            action_handle_move_command();
            return;
        }
        /* fallthrough */
    case AI_COMMAND_MOVE_SELF:
        g_battle_casting_runtime_id = g_battle_unit_source_runtime_id;
        action_handle_move_command();
        return;
    case AI_COMMAND_ACT:
        switch (unit->command_state.ai.data.action.skillset_id) {
        case SKILLSET_ID_DEFEND:
            status_inflict_defending_to_battle_id(unit->stats->battle_id);
            unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            action_set_only_action_taken(unit->stats->battle_id);
            battle_state_enter_after_command();
            return;
        case SKILLSET_ID_EQUIP_CHANGE:
            unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
            status_enable_disable_acting(unit->stats);
            ai_init_unit_abilities(unit->stats->battle_id);
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            action_set_only_action_taken(unit->stats->battle_id);
            battle_state_enter_after_command();
            return;
        case SKILLSET_ID_BLANK_04:
        case SKILLSET_ID_MIMIC:
            action_set_only_action_taken(unit->stats->battle_id);
            battle_state_enter_after_command();
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
            unit->used_ability_id = unit->command_state.ai.data.action.ability_id;
            result = target_set_panels_for_action(&unit->command_state.ai.data.action);
            unit->ability_preview_phase = result;
            switch (result) {
            case TARGET_PANELS_UNIT_OR_TILE:
            case TARGET_PANELS_TILE_ONLY:
            case TARGET_PANELS_NONE:
                battle_state_enter_action_execution_setup();
                return;
            case TARGET_PANELS_NO_TARGETING:
            default:
                target_select_tile();
                return;
            }
        }
    case AI_COMMAND_END_TURN:
        break;
    default:
        return;
    }
    unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_MOVED | BATTLE_UNIT_TURN_ACTION_ACTED;
    action_end_turn(unit->stats->battle_id);
    battle_state_enter_after_command();
}

/* ActionHelpMenu game-state handler: when the help window has gone, reopen the
 * idle action menu, the active unit's when the casting unit is the source unit
 * and the non-active unit's otherwise. */
void battle_state_handle_action_help_menu_state(void) {
    battle_unit_t* unit;

    if (menu_get_thread_activity_variant() != 2) {
        g_battle_menu_help_opening = 0;
        unit = battle_unit_get_casting();
        if (unit != 0) {
            if (unit->runtime_id == g_battle_unit_source_runtime_id) {
                battle_menu_open_active_unit_idle_action_menu();
            } else {
                battle_menu_open_non_active_unit_idle_action_menu();
            }
        }
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* DisplayMoveArea game-state handler, a unit's move range shown from the free
 * cursor: cross clears the marks, restores the cursor input mode and returns to
 * the free cursor. */
void battle_state_handle_display_move_area_state(void) {
    if (g_main_input_pressed_buttons & PSX_PAD_CROSS) {
        target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 1);
        g_battle_cursor_input_mode = g_battle_cursor_input_mode_saved;
        main_sound_play_sfx(MAIN_SFX_CANCEL);
        battle_state_set_free_cursor();
    }
    camera_handle_rotation_input();
    camera_update_zoom_input();
    camera_update_tilt_input();
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* MiniMenu game-state handler. Select opens the help window. A menu result of 0
 * (closed) restores the cursor movement option and returns to the free cursor; a
 * result of 0x64 or more shows the unit of AT-list entry result - 0x64. */
void battle_state_handle_mini_menu_state(void) {
    s32 menu_result;

    menu_result = menu_get_thread_activity_variant();
    if (g_main_input_pressed_buttons & PSX_PAD_SELECT) {
        battle_menu_open_mini_menu_help();
    } else if (menu_result == 0) {
        g_battle_cursor_input_mode = g_main_game_options.fields.cursor_movement;
        battle_state_set_free_cursor();
    } else if (menu_result >= 0x64) {
        action_get_next_acting_unit(menu_result - 0x64);
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* MiniMenuHelp game-state handler: when the help window has gone, reopen the mini
 * menu. */
void battle_state_handle_mini_menu_help_state(void) {
    if (menu_get_thread_activity_variant() != 2) {
        g_battle_menu_help_opening = 0;
        battle_menu_open_free_cursor_mini_menu();
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* ActiveTurn game-state handler, the view of a unit picked from the AT list:
 * circle or cross returns to the mini menu. */
void battle_state_handle_active_turn_state(void) {
    s32 controller_input;

    controller_input = g_main_input_pressed_buttons;
    if ((controller_input & PSX_PAD_CIRCLE) || (controller_input & PSX_PAD_CROSS)) {
        battle_menu_open_mini_menu();
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* UnitMove game-state handler: command 7 goes on to choosing a destination
 * (battle_menu_close_move_help); 8 or 0xff returns to the idle action menu. */
void battle_state_handle_unit_move_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    switch (selected_command) {
    case 7:
        battle_menu_close_move_help();
        return;
    case 8:
    case 0xff:
        battle_menu_open_active_unit_idle_action_menu();
        return;
    }
}

/* Free-cursor movement step. With the acting unit under player control the
 * confirm button routes the cursor to the pathfinder and, on a reachable
 * destination, installs the returned step list on the casting record and
 * enters the moving state. Without player control the same happens from the
 * unit's own stored target after 0x1f frames. */
void battle_state_handle_close_move_help_state(void) {
    battle_unit_t* source;
    battle_unit_t* casting;
    battle_walk_path_t* path;
    s32 buttons;
    u16 frame;
    s32 x;
    s32 elevation;
    s32 y;
    battle_unit_command_state_t scratch;

    source = battle_unit_get_source();
    casting = battle_unit_get_casting();
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    if (source->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
        target_move_cursor_by_input();
        camera_handle_rotation_input();
        camera_update_zoom_input();
        camera_update_tilt_input();
        buttons = g_main_input_pressed_buttons;
        if (buttons & PSX_PAD_CIRCLE) {
            path = move_build_path_to_tile(
                casting->stats->battle_id, g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
            if (path != 0) {
                /* Copy the 0x80-byte movement block in the renderer-side runtime record. */
                casting->path = *path;
                casting->walk_speed = 0x2000;
                target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 1);
                main_sound_play_sfx(MAIN_SFX_CONFIRM);
                battle_state_enter_unit_moving_setup();
                return;
            }
            battle_menu_open_illegal_move_help();
            return;
        }
        if (buttons & PSX_PAD_CROSS) {
            target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 1);
            target_move_cursor_to_unit(source);
            battle_menu_open_active_unit_idle_action_menu();
            return;
        }
        if (buttons & PSX_PAD_TRIANGLE) {
            target_move_cursor_to_unit(battle_unit_get_source());
            target_store_cursor_unit_name_and_data();
        }
        return;
    }
    frame = source->state_frame_counter;
    source->state_frame_counter = frame + 1;
    if (frame < 0x1f) {
        return;
    }
    path = move_build_path_to_tile(casting->stats->battle_id, source->command_state.ai.data.move.x,
        source->command_state.ai.data.move.y, source->command_state.ai.data.move.layer);
    target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 1);
    if (path != 0) {
        x = source->command_state.ai.data.move.x;
        elevation = source->command_state.ai.data.move.layer;
        y = source->command_state.ai.data.move.y;
        g_battle_cursor_x = x;
        g_battle_cursor_z = elevation;
        g_battle_cursor_y = y;
        casting->path = *path;
        casting->walk_speed = 0x2000;
        battle_state_enter_unit_moving_setup();
        return;
    }
    battle_menu_set_next_script_action_menus();
}

/* MoveRangeException game-state handler: when the message closes (command 7, 8 or
 * 0xff), reopen the idle action menu of the unit under the cursor. */
void battle_state_handle_move_range_exception_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        battle_menu_dispatch_idle_action_menu();
    }
}

/* IllegalMoveMenu game-state handler: when the message closes (command 7, 8 or
 * 0xff), return to choosing a destination (battle_menu_close_move_help). */
void battle_state_handle_illegal_move_menu_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        battle_menu_close_move_help();
    }
}

/* UnitMovingSetup game-state handler: command 7 starts the move; 8 or 0xff
 * discards the path and returns to choosing a destination. */
void battle_state_handle_unit_moving_setup_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    battle_unit_t* casting_unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    casting_unit = battle_unit_get_casting();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case 7:
        battle_state_enter_unit_moving();
        return;
    case 8:
    case 0xff:
        casting_unit->path.count = 0;
        battle_menu_close_move_help();
        return;
    }
}

/* UnitMoving game-state handler: advance the casting unit along its path. When
 * the last step has ended and no animation is pending, refresh its display and
 * open the move confirmation. */
void battle_state_handle_unit_moving_state(void) {
    battle_unit_t* casting_unit;

    camera_handle_rotation_input();
    camera_update_zoom_input();
    camera_update_tilt_input();
    casting_unit = battle_unit_get_casting();
    move_update_path_step(casting_unit);
    if (casting_unit->step_phase == MOVE_STEP_PHASE_NONE
        && casting_unit->movement_path_offset >= casting_unit->path.count
        && g_battle_state_animation_continue_check == 0) {
        battle_unit_update_display_by_runtime_id(casting_unit->runtime_id);
        battle_menu_open_move_confirm();
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}
