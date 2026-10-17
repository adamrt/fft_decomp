/* The state handlers from the move confirm menu through attack execution, target selection, turn changes, effects and
 * map jump transitions. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/ai.h"
#include "fft/ai_command.h"
#include "fft/ai_decision.h"
#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/battle_map_load.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/crystal.h"
#include "fft/cursor.h"
#include "fft/effect_script.h"
#include "fft/fade.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/move.h"
#include "fft/move_step.h"
#include "fft/result.h"
#include "fft/reward.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/state.h"
#include "fft/system.h"
#include "fft/target.h"
#include "fft/turn.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Put the unit back where it was before its move: restore its tile, facing, mount link and
 * standing-on id from the previous_* fields, re-register the tile in its battle stats and restart
 * its current animation. */
static void move_restore_previous_state(unit_t* unit) {
    unit->map_x = unit->previous_map_x;
    unit->map_y = unit->previous_map_y;
    unit->map_z = unit->previous_map_z;
    unit->facing = unit->previous_facing;
    unit->mount_state = unit->previous_mount_state;
    unit->mount_partner_runtime_id = unit->previous_mount_partner_runtime_id;
    unit->standing_on_battle_id_plus_one = unit->previous_standing_on_battle_id_plus_one;
    unit_set_move_and_world_coords(unit);
    unit_set_tile_position(
        unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, (unit->facing / UNIT_FACING_QUARTER_TURN));
    animation_force_facing(unit->animation.sequence_id >> 1, unit->facing, unit);
}

/* Cancel a move at the confirm prompt. The unit returns to its previous tile;
 * a mount takes its rider back with it, a unit that was riding is put back on
 * its mount, and a mount it climbed onto during the move is freed. */
static void move_undo(unit_t* unit) {
    unit_t* other;

    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        other = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        other->mount_state = UNIT_MOUNT_STATE_NONE;
        unit_update_display_by_runtime_id(other->runtime_id);
    }
    move_restore_previous_state(unit);
    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        other = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (other != 0) {
            move_restore_previous_state(other);
            unit_update_display_by_runtime_id(other->runtime_id);
        }
    } else if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        unit->status_flags_1_4 |= UNIT_STATUS_MOUNTED;
        other = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (other != 0) {
            other->mount_state = UNIT_MOUNT_STATE_MOUNT;
            other->mount_partner_runtime_id = unit->runtime_id;
            unit_update_display_by_runtime_id(other->runtime_id);
        }
    } else {
        unit->status_flags_1_4 &= ~UNIT_STATUS_MOUNTED;
    }
    unit_update_display_by_runtime_id(unit->runtime_id);
}

/*
 * Keep the move chosen at the confirm prompt: the moved unit re-registers its
 * tile and, when it carries a rider (mount state MOUNT), the rider takes its
 * tile, destination, and real and world position raised 10 units (0xa000 in
 * Q12). Then the acting unit's move is marked taken and the post-movement
 * steps start.
 */
static void move_confirm(unit_t* unit, unit_t* moved) {
    unit_t* rider;

    if (moved->stats != 0) {
        unit_set_tile_position(moved->stats->battle_id, moved->map_x, moved->map_y, moved->map_z,
            (moved->facing / UNIT_FACING_QUARTER_TURN));
        if (moved->mount_state == UNIT_MOUNT_STATE_MOUNT) {
            rider = unit_get_by_runtime_id(moved->mount_partner_runtime_id);
            if (rider != 0) {
                rider->map_x = rider->movement.destination_x = moved->map_x;
                rider->map_y = rider->movement.destination_y = moved->map_y;
                rider->map_z = rider->movement.destination_z = moved->map_z;
                rider->real = moved->real;
                rider->world_coords = moved->world_coords;
                rider->world_coords.vy -= 10;
                rider->real.vy -= 0xa000;
                unit_set_tile_position(rider->stats->battle_id, moved->map_x, moved->map_y, moved->map_z,
                    (moved->facing / UNIT_FACING_QUARTER_TURN));
            }
        }
    }
    unit->turn_action_flags |= TURN_ACTION_MOVED;
    turn_set_move_taken(unit->stats->battle_id);
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) && !(unit->path.flags & MOVE_EFFECTIVE_TELEPORT)) {
        sound_play_sfx(SOUND_SFX_CONFIRM);
    }
    sprite_palette_restore_all();
    move_start_post_events();
}

/* MoveConfirmMenu game-state handler. YES keeps the move and finishes it. NO or UNRESOLVED_FF
 * undoes it and returns to choosing a destination, except after a teleport, which is kept as with
 * YES. */
void state_handle_move_confirm_menu(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    unit_t* source_unit;
    unit_t* casting_unit;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command_address = command_get_selected();
    source_unit = unit_get_source();
    casting_unit = unit_get_casting();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
        if (casting_unit->path.flags & MOVE_EFFECTIVE_TELEPORT) {
        case AI_COMMAND_PROMPT_YES:
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
    ai_command_t* selected_command_address;
    s32 selected_command;
    unit_t* source_unit;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command_address = command_get_selected();
    source_unit = unit_get_source();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
        if (turn_is_over(source_unit->stats->battle_id) != 1) {
            state_enter_after_command();
            return;
        }
    case AI_COMMAND_PROMPT_YES:
        state_enter_wait_direction();
        return;
    }
}

/* Write the unit's tile and facing back to its battle stats; a unit with a mount partner also
 * gives the partner its facing and the same tile. */
static void unit_set_tile_position_with_partner(unit_t* unit) {
    unit_t* mounted_unit;

    unit_set_tile_position(
        unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, (unit->facing / UNIT_FACING_QUARTER_TURN));

    if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
        mounted_unit = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (mounted_unit != 0) {
            mounted_unit->facing = unit->facing;
            mounted_unit->camera_facing_quadrant = unit->camera_facing_quadrant;
            mounted_unit->camera_facing_sixteenth = unit->camera_facing_sixteenth;

            unit_set_tile_position(mounted_unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z,
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
    unit_t* unit = unit_get_source();
    unit_t* mounted_unit;

    sprite_project_all_units();
    if (g_menu_status_screen_selected != 1) {
        unit_update_and_animate_all();
    }
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        unit_facing_draw_wait_direction();
    }
    sprite_update_status_bubbles_and_graphics();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        camera_rotation_handle_input();
        camera_zoom_handle_input();
        camera_tilt_handle_input();
        if ((g_input_pressed_buttons & PSX_PAD_CIRCLE) != 0) {
            if (unit->stats != 0) {
                turn_end(unit->stats->battle_id);
                unit_set_tile_position_with_partner(unit);
            }
            sound_play_sfx(SOUND_SFX_CONFIRM);
            turn_start_next_event();
        } else if ((g_input_pressed_buttons & PSX_PAD_CROSS) != 0) {
            state_enter_after_command();
        }
        /* The target passes only the mode; the definition's second parameter is a matching device. */
        facing = camera_get_input_direction(CAMERA_INPUT_PRESSED);
        if (facing != -1 && facing != g_state_wait_direction_facing) {
            sound_play_sfx(SOUND_SFX_CURSOR_MOVE);
            g_state_wait_direction_facing = facing;
        }
        unit_facing_set_cardinal(unit, facing);
        if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
            mounted_unit = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
            if (mounted_unit != 0) {
                unit_facing_set_cardinal(mounted_unit, facing);
            }
        }
    } else {
        facing_hint = unit->command_state.cursor.facing_hint;
        if (facing_hint < 4) {
            unit_facing_set_cardinal(unit, facing_hint * UNIT_FACING_QUARTER_TURN);
            if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
                mounted_unit = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
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
                unit_set_tile_position_with_partner(unit);
            }
            turn_start_next_event();
        }
    }
}

/*
 * CrystalLearn game-state handler: apply the crystal or treasure choice
 * (0, LEARN_ABILITIES and TAKE_TREASURE pass their value to
 * crystal_apply_choice, RESTORE_HP_MP restores HP and MP and
 * finalizes the action, and the prompt answers only close). Then, once no effect is pending, the casting unit's
 * frame counter has reached 0x1f and no numeric display shows, work on the first
 * pending post-movement event of g_move_post_events, clearing it once
 * done, and start the next post-movement step.
 */
void state_handle_post_move_events(void) {
    ai_command_t* command;
    unit_t* source;
    unit_t* casting;
    u16 counter;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    command = command_get_selected();
    source = unit_get_source();
    casting = unit_get_casting();
    switch ((s32)command->kind) {
    case CRYSTAL_RESTORE_HP_MP:
        crystal_apply_choice(source->stats, CRYSTAL_RESTORE_HP_MP);
        g_state_window_closed = 1;
        action_apply_target_result(source->stats->battle_id);
        result_prepare_display(source);
        unit_update_display_by_runtime_id(source->runtime_id);
        break;
    case 0:
    case CRYSTAL_LEARN_ABILITIES:
    case CRYSTAL_TAKE_TREASURE:
        crystal_apply_choice(source->stats, command->kind);
        g_state_window_closed = 1;
        break;
    case AI_COMMAND_PROMPT_YES:
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
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
        if (unit_get_crystal_or_treasure_at_map_coords(source->map_x, source->map_y, source->map_z) != 0) {
            return;
        }
        g_move_post_events &= ~MOVE_POST_EVENT_CRYSTAL_OR_TREASURE;
        ai_init_selected_action();
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
        ai_init_selected_action();
        unit_update_display_by_runtime_id(source->runtime_id);
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
 * (target_begin_tile_selection); NO or UNRESOLVED_FF returns to the idle action menu. */
void state_handle_targeting_prompt(void) {
    s32 selected_command;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    switch (selected_command) {
    case AI_COMMAND_PROMPT_YES:
        target_begin_tile_selection();
        return;
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
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
    unit_t* unit = unit_get_source();
    unit_t* target;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        cursor_move_by_input();
        camera_rotation_handle_input();
        camera_zoom_handle_input();
        camera_tilt_handle_input();
        if ((g_input_pressed_buttons & PSX_PAD_CROSS) != 0) {
            target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 2);
            menu_action_open_active_unit();
            return;
        }
        if ((g_input_pressed_buttons & PSX_PAD_CIRCLE) != 0) {
            if ((battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_z)->ceiling_depth_and_marks.ability_range)
                != 0) {
                target_select_tile();
                return;
            }
            state_enter_illegal_range();
        }
    } else {
        switch (unit->command_state.ai.data.action.targeting_type) {
        case TARGET_TYPE_TILE:
            cursor_move_to_selection(unit, 0);
            frame = unit->state_frame_counter++;
            if (frame >= 0x1f) {
                /* This lookup and the one below discard their result. */
                battle_map_tile_get_pointer(unit->command_state.cursor.target_panel.vx,
                    unit->command_state.cursor.target_panel.vz, unit->command_state.cursor.target_panel.vy);
                target_select_tile();
            }
            return;
        case TARGET_TYPE_UNIT:
            target = unit_get_by_battle_id(unit->command_state.ai.data.action.target_id);
            if (target != 0) {
                cursor_move_to_selection(unit, target);
                frame = unit->state_frame_counter++;
                if (frame >= 0x1f) {
                    battle_map_tile_get_pointer(target->map_x, target->map_y, target->map_z);
                    target_select_tile();
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
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        target_begin_tile_selection();
    }
}

/* AbilityPreviewHelp game-state handler: when the help window has gone
 * (menu_get_thread_activity returns something other than 2), return to the
 * ability preview with map panning. */
void state_handle_ability_preview_help(void) {
    if (menu_get_thread_activity() != 2) {
        g_state_in_help = 0;
        camera_pan_enable();
        g_game_state = GAME_STATE_ABILITY_PREVIEW_HANDLING;
    }
    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
}

/* AbilityPreviewHandling game-state handler: the pending ability's targets are
 * marked and the unit confirms or backs out.
 *
 * For a player-controlled unit circle restores the cursor input mode and opens
 * the confirmation prompt (state_enter_confirm_action). Cross clears the target marks
 * and returns to the idle action menu when the ability has no target choice,
 * otherwise to the range display (target_begin_tile_selection). An AI-controlled
 * unit confirms by itself once its state frame counter reaches 0x1f. */
void state_handle_ability_preview(void) {
    u16 frame;
    unit_t* unit = unit_get_source();

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
            target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_HIDE_TARGETS, 3);
            if (unit->ability_preview_phase == TARGET_PANELS_NO_TARGETING) {
                target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 3);
                sprite_palette_restore_all();
                menu_action_open_active_unit();
            } else {
                target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 3);
                sprite_palette_restore_all();
                target_begin_tile_selection();
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
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
}

/* ConfirmAction game-state handler. The answers double as targeting types. For a player-controlled
 * unit, TARGET_TYPE_TILE aims the action at the cursor tile, and TARGET_TYPE_UNIT
 * or a plain YES at the unit on it (the tile when it is empty); the answer is stored as the
 * targeting type as it is, and both store the cursor tile and start the pre-attack animation. NO or
 * UNRESOLVED_FF clears the marks and returns to the range display. An AI unit starts the pre-attack
 * animation on TILE, UNIT or YES. */
void state_handle_confirm_action(void) {
    unit_t* unit;
    ai_command_t* selected_command_address;
    unit_t* target;
    s32 selected_command;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command_address = command_get_selected();
    unit = unit_get_source();
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        selected_command = selected_command_address->kind;
        switch (selected_command) {
        case TARGET_TYPE_TILE:
            unit->command_state.ai.data.action.targeting_type = selected_command_address->kind;
            vector_set_svector(&unit->command_state.cursor.target_panel, g_cursor_x, g_cursor_z, g_cursor_y);
            state_enter_pre_attack_animation();
            return;
        case TARGET_TYPE_UNIT:
        case AI_COMMAND_PROMPT_YES:
            target = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_z);
            if (target != 0) {
                unit->command_state.ai.data.action.targeting_type = selected_command_address->kind;
                unit->command_state.ai.data.action.target_id = target->stats->battle_id;
            } else {
                unit->command_state.ai.data.action.targeting_type = TARGET_TYPE_TILE;
            }
            vector_set_svector(&unit->command_state.cursor.target_panel, g_cursor_x, g_cursor_z, g_cursor_y);
            state_enter_pre_attack_animation();
            return;
        case AI_COMMAND_PROMPT_NO:
        case AI_COMMAND_UNRESOLVED_FF:
            target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 3);
            target_begin_tile_selection();
            return;
        default:
            break;
        }
    } else {
        selected_command = selected_command_address->kind;
        switch (selected_command) {
        case TARGET_TYPE_TILE:
        case TARGET_TYPE_UNIT:
        case AI_COMMAND_PROMPT_YES:
            state_enter_pre_attack_animation();
            return;
        case AI_COMMAND_PROMPT_NO:
        case AI_COMMAND_UNRESOLVED_FF:
            target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 3);
            target_begin_tile_selection();
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
    unit_t* unit;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    unit = unit_get_source();
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
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    /* Any prompt answer marks the window closed (the same test as the
     * BATTLE_MESSAGE_DISPLAY handler). */
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
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
    unit_t* unit;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    unit = unit_get_source();
    if (unit->attack_phase_state == ACTION_PREPARATION_JUMP) {
        if (unit->animation.wait == 0) {
            turn_set_act_taken(unit->stats->battle_id);
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
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
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
    unit_t* unit = unit_get_casting();
    s32 open_result;
    s32 slot;
    s32 free_owner;
    s32 offset;
    RECT* image_rect_address;
    u32* sp2_data;

    if (g_sprite_sp2_data == 0) {
        g_sprite_sp2_data = heap_alloc(0x8000);
        open_result = sprite_open_sp2_file(unit, g_sprite_sp2_data);
        if (open_result == -1) {
            heap_free(g_sprite_sp2_data);
            g_sprite_sp2_data = 0;
            action_start_next_strike();
        } else if (open_result == 0) {
            heap_free(g_sprite_sp2_data);
            g_sprite_sp2_data = 0;
        }
    } else if (file_is_still_loading() == 0) {
        slot = 0;
        free_owner = SPRITE_EVTCHR_SLOT_OWNER_NONE;
        /* Keep the target's byte-offset induction; advancing a slot pointer changes the exact code. */
        offset = 0;
        do {
            if (((sprite_evtchr_slot_t*)((u8*)g_sprite_evtchr_slots + offset))->owner == free_owner) {
                image_rect_address = &image_rect;
                sp2_data = g_sprite_sp2_data;
                ((sprite_evtchr_slot_t*)((u8*)g_sprite_evtchr_slots + offset))->owner
                    = unit->runtime_id | SPRITE_EVTCHR_SLOT_OWNER_SP2;
                image_rect_address->x = ((sprite_evtchr_slot_t*)((u8*)g_sprite_evtchr_slots + offset))->image_rect.x;
                image_rect_address->y = ((sprite_evtchr_slot_t*)((u8*)g_sprite_evtchr_slots + offset))->image_rect.y;
                image_rect_address->w = 0x40;
                image_rect_address->h = 0x100;
                LoadImage(image_rect_address, sp2_data);
                action_start_next_strike();
                break;
            }
            slot++;
            offset += sizeof(sprite_evtchr_slot_t);
        } while (slot < 2);
    }
    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
}

/* StartEffectFileOpen game-state handler: step the effect-file load
 * (effect_file_update_load); when no work remains, resolve the strike and pick
 * the attacker's animation. */
void state_handle_start_effect_file_open(void) {
    if (effect_file_update_load() != 0) {
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
    unit_t* unit = unit_get_casting();

    unit->state_frame_counter++;
    state_enter_action_execute();
    state_update_units();
}

/* Start the target's reaction animation for the attacker's attack and prepare its post-action
 * display; nothing happens when either unit is missing. */
static void action_start_target_animation_display(unit_t* attacker, unit_t* target) {
    if (attacker != 0 && target != 0) {
        action_start_target_response(attacker, target);
        result_prepare_display_by_runtime_id(target->runtime_id);
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
    unit_t* unit;
    unit_t* target;
    s32 i;

    sprite_project_all_units();
    if (g_menu_status_screen_selected != 1) {
        unit_update_and_animate_all();
        fade_tint_update_fade();
        camera_real_coords_update_animation();
        camera_screen_offset_update_animation();
        camera_zoom_update_animation();
        camera_rotation_update_animation();
    }
    sprite_update_status_bubbles_and_graphics();
    fade_tint_draw_overlay();
    cursor_update();

    unit = unit_get_casting();
    /* The target reads the 16-bit value of g_system_vsync_interval. */
    unit->state_frame_counter += g_system_vsync_interval;
    if (unit->strike.target_count != 0) {
        for (i = 0; i < unit->strike.target_count; i++) {
            target = unit_get_by_battle_id(unit->strike.target_list[i]);
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
            camera_update_cursor_tile_vector();
        }
        if (g_state_animation_continue_check == 0 && unit->numeric_display.active == 0
            && (unit->animation.wait == 0 || (unit->animation.sequence_id >> 1) < ANIMATION_ATTACK_FIRST)) {
            sound_replay_background();
            if (unit->strike.target_count != 0) {
                for (i = 0; i < unit->strike.target_count; i++) {
                    target = unit_get_by_battle_id(unit->strike.target_list[i]);
                    if (target != 0) {
                        if (unit->used_ability_id >= ABILITY_ID_REACTION_FIRST
                            && unit->used_ability_id < ABILITY_ID_SUPPORT_FIRST) {
                            if (effect_ability_has_no_file(unit->used_ability_id) != 0
                                && g_ability_animation_data[unit->used_ability_id].attack_animation
                                    != ANIMATION_ATTACK_USE_WEAPON) {
                                action_start_target_animation_display(unit, target);
                            }
                        } else {
                            unit_update_display_by_runtime_id(target->runtime_id);
                        }
                    }
                }
            }
            state_enter_battle_message_display();
        }
    } else {
        camera_update_cursor_tile_vector();
        menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
        if (g_state_animation_continue_check == 0 && unit->numeric_display.active == 0
            && (unit->animation.wait == 0 || (unit->animation.sequence_id >> 1) < ANIMATION_ATTACK_FIRST)) {
            if (unit->strike.target_count != 0) {
                for (i = 0; i < unit->strike.target_count; i++) {
                    target = unit_get_by_battle_id(unit->strike.target_list[i]);
                    if (target != 0) {
                        if (unit->used_ability_id == ABILITY_ID_KNOCKBACK) {
                            result_prepare_display_by_runtime_id(target->runtime_id);
                        }
                        unit_update_display_by_runtime_id(target->runtime_id);
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
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
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
    unit_t* unit;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        g_state_window_closed = 1;
    }
    unit = unit_get_casting();
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
        animation_call_set_for_status(unit);
        unit_update_target_displays(unit);
        unit_get_source(); /* result unused */
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
    ai_command_t* selected_command_address;
    s32 selected_command;
    s32 i;
    s32 display_active;
    s32 target_count;
    unit_t* casting_unit;
    unit_t* target_unit;
    u8 compiler_stack_pad[8];

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command_address = command_get_selected();
    casting_unit = unit_get_casting();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case AI_COMMAND_PROMPT_YES:
        if (g_reward_report_phase == REWARD_REPORT_LEARN_ON_HIT) {
            target_unit = unit_get_by_battle_id(g_ability_learn_on_hit_unit_id);
            ability_learn_used(target_unit->stats);
        }
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
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
                    target_unit = unit_get_by_battle_id(casting_unit->strike.target_list[i]);
                    if (target_unit != 0 && target_unit->numeric_display.active != 0) {
                        display_active = 1;
                        break;
                    }
                    target_count = casting_unit->strike.target_count;
                    i += 1;
                } while (i < target_count);
            }
            if (display_active == 0) {
                casting_unit = unit_get_source();
                animation_call_set_for_status(casting_unit);
                action_end_command(casting_unit);
            }
        }
    }
}

/* Frame step shared by the two Deep Dungeon mesh states: run the unit and menu update and, once
 * the map data has loaded, refresh every unit's display, clear
 * g_battle_map_deep_dungeon_load_in_progress and return 1. Returns 0 while the load continues. */
static s32 battle_map_deep_dungeon_update_mesh_load(void) {
    s32 runtime_id;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    if (battle_map_load_poll() != 0) {
        return 0;
    }
    for (runtime_id = 0; runtime_id < UNIT_RUNTIME_SLOT_COUNT; runtime_id++) {
        unit_t* unit = unit_get_by_runtime_id(runtime_id);
        if (unit != 0) {
            unit_update_display_by_runtime_id(unit->runtime_id);
        }
    }
    g_battle_map_deep_dungeon_load_in_progress = 0;
    return 1;
}

/* DeepDungeonMeshLoad game-state handler: when the Deep Dungeon map has loaded,
 * run the between-turn events. */
void state_handle_deep_dungeon_mesh_load(void) {
    if (battle_map_deep_dungeon_update_mesh_load() != 0) {
        turn_start_next_event();
    }
}

/* DeepDungeonMeshFinish game-state handler: when the Deep Dungeon map has loaded,
 * enter the after-command state. */
void state_handle_deep_dungeon_mesh_finish(void) {
    if (battle_map_deep_dungeon_update_mesh_load() != 0) {
        state_enter_after_command();
    }
}

/* TargetSelectStart game-state handler: when the prompt closes (any answer), enter target
 * selection. */
void state_handle_target_select_start(void) {
    s32 selected_command;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        state_enter_target_select();
    }
}

/* TargetSelect game-state handler: the player picks the auto-battle target with the cursor. Circle
 * confirms the unit on the tile when it suits the choice (an enemy for the targeted choice, an
 * ally for the recovery choice) and denies it otherwise or on an empty tile; cross moves the
 * cursor back to the casting unit and reopens the action menus, and triangle only moves the cursor
 * back. */
void state_handle_target_select(void) {
    unit_t* caster;
    unit_t* target;

    camera_rotation_handle_input();
    camera_zoom_handle_input();
    camera_tilt_handle_input();
    cursor_move_by_input();
    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    if (g_input_pressed_buttons & PSX_PAD_CIRCLE) {
        caster = unit_get_casting();
        target = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_z);
        /* The targeted choice requires an enemy, the recovery choice an ally. */
        if (target == 0) {
            state_enter_target_select_denied();
        } else if (caster->target_select_command == AI_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT) {
            if ((caster->stats->team_flags & UNIT_TEAM_MASK) == (target->stats->team_flags & UNIT_TEAM_MASK)) {
                state_enter_target_select_denied();
            } else {
                state_enter_target_select_confirm();
            }
        } else if (caster->target_select_command == AI_COMMAND_AUTOBATTLE_PROTECT_ALLIES_SELECT) {
            if ((caster->stats->team_flags & UNIT_TEAM_MASK) == (target->stats->team_flags & UNIT_TEAM_MASK)) {
                state_enter_target_select_confirm();
            } else {
                state_enter_target_select_denied();
            }
        }
        sprite_palette_restore_all();
    } else if (g_input_pressed_buttons & PSX_PAD_CROSS) {
        cursor_move_to_unit(unit_get_casting());
        sprite_palette_restore_all();
        state_enter_open_action_menus();
    } else if (g_input_pressed_buttons & PSX_PAD_TRIANGLE) {
        cursor_move_to_unit(unit_get_casting());
    }
}

/* TargetSelectDenied game-state handler: when the message closes (any answer), return to target
 * selection. */
void state_handle_target_select_denied(void) {
    s32 selected_command;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        state_enter_target_select();
    }
}

/* TargetSelectConfirm game-state handler: YES stores the unit under the cursor as the casting
 * unit's auto-battle target and opens the next action menus; NO or UNRESOLVED_FF returns to target
 * selection. */
void state_handle_target_select_confirm(void) {
    unit_t* caster;
    unit_t* target;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    switch ((s32)command_get_selected()->kind) {
    case AI_COMMAND_PROMPT_YES:
        caster = unit_get_casting();
        target = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_z);
        auto_battle_store_target(target->stats->battle_id, caster->stats->battle_id);
        cursor_move_to_unit(caster);
        state_enter_open_action_menus();
        break;
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
        state_enter_target_select();
        break;
    }
}

/* TargetDisplayStart game-state handler: once the prompt has closed (any answer) and the camera has
 * settled, enter the target display. */
void state_handle_target_display_start(void) {
    s32 selected_command;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        g_state_window_closed = 1;
    }
    if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0) {
        if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
            if (g_state_window_closed != 0) {
                state_enter_target_display();
            }
        }
    }
}

/* TargetDisplay game-state handler: circle or cross puts the cursor back on the
 * casting unit and opens the next action menus. */
void state_handle_target_display(void) {
    s32 buttons;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    buttons = g_input_pressed_buttons;
    if ((buttons & PSX_PAD_CIRCLE) || (buttons & PSX_PAD_CROSS)) {
        cursor_move_to_unit(unit_get_casting());
        state_enter_open_action_menus();
    }
}

/* AfterCommand game-state handler: once the window has closed (any answer) and the source unit's
 * numeric display is idle, enter the continue-turn state. */
void state_handle_after_command(void) {
    ai_command_t* command_address;
    s32 command;
    unit_t* unit;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    command_address = command_get_selected();
    unit = unit_get_source();
    command = command_address->kind;
    if (command >= AI_COMMAND_PROMPT_YES && (command <= AI_COMMAND_PROMPT_NO || command == AI_COMMAND_UNRESOLVED_FF)) {
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
    unit_t* unit;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    unit = unit_get_source();
    if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) != 0) {
        return;
    }
    if (g_camera_rotation_action != CAMERA_ROTATION_NONE) {
        return;
    }
    if (g_turn_event_type == TURN_EVENT_ABILITY_READY) {
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
        turn_start_next_event();
        return;
    }
    if (turn_is_over(unit->stats->battle_id) == 1) {
        /* The stats keep a copy of the command the unit acted on. */
        unit->command_state.ai.data.action = unit->stats->action_command;
        unit->used_ability_id = unit->stats->action_command.ability_id;
        if (unit->attack_phase_state == ACTION_PREPARATION_JUMP) {
            unit->attack_phase_state = ACTION_PREPARATION_CHARGE;
            unit_hide_by_runtime_id(unit->runtime_id);
            turn_start_next_event();
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
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    command = command_get_selected()->kind;
    if (command >= AI_COMMAND_PROMPT_YES) {
        if (command <= AI_COMMAND_PROMPT_NO) {
            g_state_window_closed = 1;
        } else if (command == AI_COMMAND_UNRESOLVED_FF) {
            g_state_window_closed = 1;
        }
    }
    if (g_state_window_closed != 0) {
        if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) == 0) {
            if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
                unit_get_source(); /* result unused */
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
    unit_t* unit;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        g_state_window_closed = 1;
    }
    if (g_state_window_closed != 0) {
        if (g_state_animation_continue_check == 0) {
            unit = unit_get_source();
            if (unit->numeric_display.active == 0) {
                unit_update_display_by_runtime_id(unit->runtime_id);
                turn_start_next_event();
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
    sprite_project_all_units();
    if (g_menu_status_screen_selected != 1) {
        unit_update_and_animate_all();
        fade_tint_update_fade();
        camera_real_coords_update_animation();
        camera_screen_offset_update_animation();
        camera_zoom_update_animation();
        camera_rotation_update_animation();
    }
    sprite_update_status_bubbles_and_graphics();
    fade_tint_draw_overlay();
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

    sprite_project_all_units();
    if (g_menu_status_screen_selected != 1) {
        unit_update_and_animate_all();
        fade_tint_update_fade();
        camera_real_coords_update_animation();
        camera_screen_offset_update_animation();
        camera_zoom_update_animation();
        camera_rotation_update_animation();
        move_step_update_all_units();
    }
    sprite_update_status_bubbles_and_graphics();
    fade_tint_draw_overlay();
    switch (script_run_event_frame(gfx_get_battle_otag(), g_input_current_buttons)) {
    case SCRIPT_EVENT_FRAME_NO_FINISH:
        sprite_evtchr_init_slots();
        g_menu_status_enabled = 1;
        ai_init_workspace();
        turn_start_next_event();
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

        AddPrim(gfx_get_battle_otag(), &g_fade_polygons[g_gfx_screen_polarity]);
        AddPrim(gfx_get_battle_otag(), &g_fade_draw_modes[g_gfx_screen_polarity]);
    }
}

/* Stop the map's palette cycles and weather sound; every path that leaves a map calls it just
 * before unit_clear_all. */
void battle_map_stop_animations(void) {
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

    AddPrim(gfx_get_battle_otag(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_get_battle_otag(), &g_fade_draw_modes[g_gfx_screen_polarity]);

    g_fade_intensity += g_fade_step;
    if (g_fade_intensity >= 0x102) {
        g_fade_intensity = 0xff;
        g_game_state = GAME_STATE_MAP_INITIALIZE;
        g_state_map_init_step = 0;
        battle_map_stop_animations();
        unit_clear_all();
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

    AddPrim(gfx_get_battle_otag(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_get_battle_otag(), &g_fade_draw_modes[g_gfx_screen_polarity]);

    g_fade_intensity += g_fade_step;
    if (g_fade_intensity >= 0x102) {
        g_fade_intensity = 0xff;
        g_game_state = GAME_STATE_MAP_INITIALIZE_2;
        g_state_map_init_step = 0;
        battle_map_stop_animations();
        unit_clear_all();
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
    AddPrim(gfx_get_battle_otag(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_get_battle_otag(), &g_fade_draw_modes[g_gfx_screen_polarity]);
    g_fade_intensity += g_fade_step;
}

/* MapInitialize game-state handler: run one step of the map init sequence behind
 * a black screen; at step 0xd start the fade-in (MapJumpingIn). */
void state_handle_map_init(void) {
    char unused[24]; /* unreferenced; sizes the target's 0x30-byte frame */

    g_state_map_init_step = battle_map_step_init_sequence(g_battle_map_id, g_state_map_init_step);
    g_fade_polygons[g_gfx_screen_polarity].r0 = 0xff;
    g_fade_polygons[g_gfx_screen_polarity].g0 = 0xff;
    g_fade_polygons[g_gfx_screen_polarity].b0 = 0xff;
    AddPrim(gfx_get_battle_otag(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_get_battle_otag(), &g_fade_draw_modes[g_gfx_screen_polarity]);
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

    g_state_map_init_step = battle_map_step_init_sequence(g_battle_map_id, g_state_map_init_step);
    g_fade_polygons[g_gfx_screen_polarity].r0 = 0xff;
    g_fade_polygons[g_gfx_screen_polarity].g0 = 0xff;
    g_fade_polygons[g_gfx_screen_polarity].b0 = 0xff;
    AddPrim(gfx_get_battle_otag(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_get_battle_otag(), &g_fade_draw_modes[g_gfx_screen_polarity]);
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
    AddPrim(gfx_get_battle_otag(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_get_battle_otag(), &g_fade_draw_modes[g_gfx_screen_polarity]);
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

    sprite_project_all_units();
    if (g_menu_status_screen_selected != 1) {
        unit_update_and_animate_all();
        fade_tint_update_fade();
        camera_real_coords_update_animation();
        camera_screen_offset_update_animation();
        camera_zoom_update_animation();
        camera_rotation_update_animation();
        move_step_update_all_units();
    }
    sprite_update_status_bubbles_and_graphics();
    fade_tint_draw_overlay();
    /* TO_WORLD and NEXT_EVENT lead the other way round from state_handle_event
     * (QUIRKS.md). */
    switch (script_run_event_frame(gfx_get_battle_otag(), g_input_current_buttons)) {
    case SCRIPT_EVENT_FRAME_RUNNING:
        break;
    case SCRIPT_EVENT_FRAME_NO_FINISH:
        turn_start_next_event();
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

    AddPrim(gfx_get_battle_otag(), &g_fade_polygons[g_gfx_screen_polarity]);
    AddPrim(gfx_get_battle_otag(), &g_fade_draw_modes[g_gfx_screen_polarity]);

    g_fade_intensity -= g_fade_step;
    if (g_fade_intensity <= 0) {
        g_fade_intensity = 0;
        g_game_state = g_game_state_previous;
        g_game_state_previous = GAME_STATE_FREE_CURSOR;
    }
}
