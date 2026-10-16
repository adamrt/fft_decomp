/* The state handlers from the move confirm menu through attack execution, target selection, turn changes, effects and
 * map jump transitions. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_ai_command.h"
#include "fft/battle_ai_decision.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect_script.h"
#include "fft/battle_gfx.h"
#include "fft/battle_gfx_resource.h"
#include "fft/battle_map.h"
#include "fft/battle_map_resource.h"
#include "fft/battle_map_weather.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/battle_unit_animation.h"
#include "fft/battle_unit_encounter.h"
#include "fft/data_ability.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "fft/main_save.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_command.h"
#include "fft/menu_input.h"
#include "fft/menu_overlay.h"
#include "fft/script.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Put the unit back where it was before its move: restore its tile, facing, mount link and
 * standing-on id from the previous_* fields, re-register the tile in its battle stats and restart
 * its current animation. */
static void battle_move_restore_previous_state(battle_unit_t* unit) {
    unit->map_x = unit->previous_map_x;
    unit->map_y = unit->previous_map_y;
    unit->map_z = unit->previous_map_z;
    unit->facing = unit->previous_facing;
    unit->mount_state = unit->previous_mount_state;
    unit->mount_partner_runtime_id = unit->previous_mount_partner_runtime_id;
    unit->standing_on_battle_id_plus_one = unit->previous_standing_on_battle_id_plus_one;
    battle_unit_set_move_and_world_coords(unit);
    battle_unit_set_tile_position(
        unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, (unit->facing / BATTLE_FACING_QUARTER_TURN));
    battle_unit_force_animation_facing(unit->animation.sequence_id >> 1, unit->facing, unit);
}

/* Cancel a move at the confirm prompt. The unit returns to its previous tile;
 * a mount takes its rider back with it, a unit that was riding is put back on
 * its mount, and a mount it climbed onto during the move is freed. */
static void battle_move_undo_unit_move(battle_unit_t* unit) {
    battle_unit_t* other;

    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        other->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
        battle_unit_update_display_by_runtime_id(other->runtime_id);
    }
    battle_move_restore_previous_state(unit);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
        other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (other != 0) {
            battle_move_restore_previous_state(other);
            battle_unit_update_display_by_runtime_id(other->runtime_id);
        }
    } else if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        unit->status_flags_1_4 |= BATTLE_UNIT_STATUS_MOUNTED;
        other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (other != 0) {
            other->mount_state = BATTLE_UNIT_MOUNT_STATE_MOUNT;
            other->mount_partner_runtime_id = unit->runtime_id;
            battle_unit_update_display_by_runtime_id(other->runtime_id);
        }
    } else {
        unit->status_flags_1_4 &= ~BATTLE_UNIT_STATUS_MOUNTED;
    }
    battle_unit_update_display_by_runtime_id(unit->runtime_id);
}

/*
 * Keep the move chosen at the confirm prompt: the moved unit re-registers its
 * tile and, when it carries a rider (mount state MOUNT), the rider takes its
 * tile, destination, and real and world position raised 10 units (0xa000 in
 * Q12). Then the acting unit's move is marked taken and the post-movement
 * steps start.
 */
static void battle_move_confirm_unit_move(battle_unit_t* unit, battle_unit_t* moved) {
    battle_unit_t* rider;

    if (moved->stats != 0) {
        battle_unit_set_tile_position(moved->stats->battle_id, moved->map_x, moved->map_y, moved->map_z,
            (moved->facing / BATTLE_FACING_QUARTER_TURN));
        if (moved->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
            rider = battle_unit_get_by_runtime_id(moved->mount_partner_runtime_id);
            if (rider != 0) {
                rider->map_x = rider->movement.destination_x = moved->map_x;
                rider->map_y = rider->movement.destination_y = moved->map_y;
                rider->map_z = rider->movement.destination_z = moved->map_z;
                rider->real = moved->real;
                rider->world_coords = moved->world_coords;
                rider->world_coords.vy -= 10;
                rider->real.vy -= 0xa000;
                battle_unit_set_tile_position(rider->stats->battle_id, moved->map_x, moved->map_y, moved->map_z,
                    (moved->facing / BATTLE_FACING_QUARTER_TURN));
            }
        }
    }
    unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_MOVED;
    battle_action_set_only_movement_taken(unit->stats->battle_id);
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED)
        && !(unit->path.flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT)) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
    }
    battle_gfx_restore_unit_palettes();
    battle_move_start_post_movement_steps();
}

/* MoveConfirmMenu game-state handler. YES keeps the move and finishes it. NO or UNRESOLVED_FF
 * undoes it and returns to choosing a destination, except after a teleport, which is kept as with
 * YES. */
void battle_state_handle_move_confirm_menu_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    battle_unit_t* source_unit;
    battle_unit_t* casting_unit;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    source_unit = battle_unit_get_source();
    casting_unit = battle_unit_get_casting();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
        if (casting_unit->path.flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT) {
        case AI_COMMAND_PROMPT_YES:
            battle_move_confirm_unit_move(source_unit, casting_unit);
            return;
        }
        battle_move_undo_unit_move(casting_unit);
        battle_gfx_restore_unit_palettes();
        battle_state_enter_select_move_tile();
        return;
    }
}

/* WaitMenu game-state handler. YES goes on to choosing a facing. NO or UNRESOLVED_FF returns to the
 * after-command state, unless the unit's turn is over (battle_action_check_change_of_turn returns
 * 1): then it chooses a facing too. */
void battle_state_handle_wait_menu_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    battle_unit_t* source_unit;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    source_unit = battle_unit_get_source();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
        if (battle_action_check_change_of_turn(source_unit->stats->battle_id) != 1) {
            battle_state_enter_after_command();
            return;
        }
    case AI_COMMAND_PROMPT_YES:
        battle_state_enter_wait_direction();
        return;
    }
}

/* Write the unit's tile and facing back to its battle stats; a unit with a mount partner also
 * gives the partner its facing and the same tile. */
static void battle_unit_set_tile_position_with_mount_partner(battle_unit_t* unit) {
    battle_unit_t* mounted_unit;

    battle_unit_set_tile_position(
        unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, (unit->facing / BATTLE_FACING_QUARTER_TURN));

    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
        mounted_unit = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (mounted_unit != 0) {
            mounted_unit->facing = unit->facing;
            mounted_unit->camera_facing_quadrant = unit->camera_facing_quadrant;
            mounted_unit->camera_facing_sixteenth = unit->camera_facing_sixteenth;

            battle_unit_set_tile_position(mounted_unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z,
                (unit->facing / BATTLE_FACING_QUARTER_TURN));
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
 * Ending the turn applies battle_action_end_turn and re-registers the unit's tile
 * together with its mount partner. */
void battle_state_handle_wait_direction_state(void) {
    s32 facing;
    s32 facing_hint;
    u16 frame;
    battle_unit_t* unit = battle_unit_get_source();
    battle_unit_t* mounted_unit;

    battle_gfx_project_all_units();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
    }
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        battle_gfx_draw_wait_direction_unit();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        battle_camera_handle_rotation_input();
        battle_camera_update_zoom_input();
        battle_camera_update_tilt_input();
        if ((g_main_input_pressed_buttons & PSX_PAD_CIRCLE) != 0) {
            if (unit->stats != 0) {
                battle_action_end_turn(unit->stats->battle_id);
                battle_unit_set_tile_position_with_mount_partner(unit);
            }
            main_sound_play_sfx(MAIN_SFX_CONFIRM);
            battle_action_start_next_turn_event();
        } else if ((g_main_input_pressed_buttons & PSX_PAD_CROSS) != 0) {
            battle_state_enter_after_command();
        }
        /* The target passes only the mode; the definition's second parameter is a matching device. */
        facing = battle_camera_get_input_direction(CAMERA_INPUT_PRESSED);
        if (facing != -1 && facing != g_battle_state_wait_direction_facing) {
            main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
            g_battle_state_wait_direction_facing = facing;
        }
        battle_unit_set_cardinal_facing(unit, facing);
        if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
            mounted_unit = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
            if (mounted_unit != 0) {
                battle_unit_set_cardinal_facing(mounted_unit, facing);
            }
        }
    } else {
        facing_hint = unit->command_state.cursor.facing_hint;
        if (facing_hint < 4) {
            battle_unit_set_cardinal_facing(unit, facing_hint * BATTLE_FACING_QUARTER_TURN);
            if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
                mounted_unit = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                if (mounted_unit != 0) {
                    battle_unit_set_cardinal_facing(
                        mounted_unit, unit->command_state.cursor.facing_hint * BATTLE_FACING_QUARTER_TURN);
                }
            }
        }
        frame = unit->state_frame_counter++;
        if (frame >= 0x1f) {
            if (unit->stats != 0) {
                battle_action_end_turn(unit->stats->battle_id);
                battle_unit_set_tile_position_with_mount_partner(unit);
            }
            battle_action_start_next_turn_event();
        }
    }
}

/*
 * CrystalLearn game-state handler: apply the crystal or treasure choice
 * (0, LEARN_ABILITIES and TAKE_TREASURE pass their value to
 * battle_unit_apply_crystal_choice, RESTORE_HP_MP restores HP and MP and
 * finalizes the action, and the prompt answers only close). Then, once no effect is pending, the casting unit's
 * frame counter has reached 0x1f and no numeric display shows, work on the first
 * pending post-movement event of g_battle_move_post_move_events, clearing it once
 * done, and start the next post-movement step.
 */
void battle_state_handle_crystal_learn_state(void) {
    ai_command_t* command;
    battle_unit_t* source;
    battle_unit_t* casting;
    u16 counter;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    command = menu_get_selected_command_address();
    source = battle_unit_get_source();
    casting = battle_unit_get_casting();
    switch ((s32)command->kind) {
    case BATTLE_CRYSTAL_RESTORE_HP_MP:
        battle_unit_apply_crystal_choice(source->stats, BATTLE_CRYSTAL_RESTORE_HP_MP);
        g_battle_state_window_closed = 1;
        battle_action_apply_target_result(source->stats->battle_id);
        battle_gfx_prepare_post_action_display(source);
        battle_unit_update_display_by_runtime_id(source->runtime_id);
        break;
    case 0:
    case BATTLE_CRYSTAL_LEARN_ABILITIES:
    case BATTLE_CRYSTAL_TAKE_TREASURE:
        battle_unit_apply_crystal_choice(source->stats, command->kind);
        g_battle_state_window_closed = 1;
        break;
    case AI_COMMAND_PROMPT_YES:
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
        g_battle_state_window_closed = 1;
        break;
    }

    if (g_battle_state_window_closed == 0) {
        return;
    }
    if (g_battle_state_animation_continue_check != 0) {
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

    if (g_battle_move_post_move_events & MOVE_POST_EVENT_CRYSTAL_OR_TREASURE) {
        if (battle_unit_get_crystal_or_treasure_at_map_coords(source->map_x, source->map_y, source->map_z) != 0) {
            return;
        }
        g_battle_move_post_move_events &= ~MOVE_POST_EVENT_CRYSTAL_OR_TREASURE;
        battle_ai_init_selected_action();
    } else if (g_battle_move_post_move_events & MOVE_POST_EVENT_MOVEMENT_BENEFIT) {
        g_battle_action_post_action_display_phase += 1;
        if (g_battle_action_post_action_display_phase >= 2) {
            g_battle_move_post_move_events &= ~MOVE_POST_EVENT_MOVEMENT_BENEFIT;
        }
    } else if (g_battle_move_post_move_events & MOVE_POST_EVENT_ITEM_FOUND) {
        /* Wait out a non-idle animation still playing (the find reaction). */
        if ((source->animation.wait != 0) && ((source->animation.sequence_id >> 1) >= BATTLE_UNIT_ANIMATION_WALK)) {
            return;
        }
        source->item_display_active = 0;
        g_battle_move_post_move_events &= ~MOVE_POST_EVENT_ITEM_FOUND;
    } else if (g_battle_move_post_move_events & MOVE_POST_EVENT_TRAP_TRIGGERED) {
        g_battle_move_post_move_events &= ~MOVE_POST_EVENT_TRAP_TRIGGERED;
        battle_ai_init_selected_action();
        battle_unit_update_display_by_runtime_id(source->runtime_id);
    } else if (g_battle_move_post_move_events & MOVE_POST_EVENT_CHARGING_CANCEL) {
        g_battle_move_post_move_events &= ~MOVE_POST_EVENT_CHARGING_CANCEL;
    } else if (g_battle_move_post_move_events & MOVE_POST_EVENT_SOURCE_DISPLAY) {
        g_battle_move_post_move_events &= ~MOVE_POST_EVENT_SOURCE_DISPLAY;
    } else if (g_battle_move_post_move_events & MOVE_POST_EVENT_MOUNT_STATUS_CHANGE) {
        g_battle_move_post_move_events &= ~MOVE_POST_EVENT_MOUNT_STATUS_CHANGE;
    } else {
        return;
    }
    battle_move_start_next_post_movement_step();
}

/* TargetingPrompt game-state handler: YES goes on to the range display
 * (battle_target_begin_tile_selection); NO or UNRESOLVED_FF returns to the idle action menu. */
void battle_state_handle_targeting_prompt_state(void) {
    s32 selected_command;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    switch (selected_command) {
    case AI_COMMAND_PROMPT_YES:
        battle_target_begin_tile_selection();
        return;
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
        battle_menu_open_active_unit_idle_action_menu();
        return;
    }
}

/* TargetingRange game-state handler: the ability's range is shown and its target tile chosen. A
 * player-controlled unit moves the cursor and camera; cross returns to the idle action menu,
 * circle selects the tile when it is in range and otherwise enters the illegal-range state. An AI
 * unit moves the cursor to its stored target tile or unit and selects it once its state frame
 * counter reaches 0x1f; a missing target unit reopens the action menus. */
void battle_state_handle_targeting_range_state(void) {
    u16 frame;
    battle_unit_t* unit = battle_unit_get_source();
    battle_unit_t* target;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        battle_target_move_cursor_by_input();
        battle_camera_handle_rotation_input();
        battle_camera_update_zoom_input();
        battle_camera_update_tilt_input();
        if ((g_main_input_pressed_buttons & PSX_PAD_CROSS) != 0) {
            battle_target_set_tile_mark_colors(BATTLE_TILE_MARK_NONE, 2);
            battle_menu_open_active_unit_idle_action_menu();
            return;
        }
        if ((g_main_input_pressed_buttons & PSX_PAD_CIRCLE) != 0) {
            if ((battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z)
                        ->ceiling_depth_and_marks.ability_range)
                != 0) {
                battle_target_select_tile();
                return;
            }
            battle_state_enter_target_out_of_range();
        }
    } else {
        switch (unit->command_state.ai.data.action.targeting_type) {
        case BATTLE_TARGETING_TYPE_TILE:
            battle_target_move_cursor_to_selection(unit, 0);
            frame = unit->state_frame_counter++;
            if (frame >= 0x1f) {
                /* This lookup and the one below discard their result. */
                battle_map_get_tile_data_pointer(unit->command_state.cursor.target_panel.vx,
                    unit->command_state.cursor.target_panel.vz, unit->command_state.cursor.target_panel.vy);
                battle_target_select_tile();
            }
            return;
        case BATTLE_TARGETING_TYPE_UNIT:
            target = battle_unit_get_by_battle_id(unit->command_state.ai.data.action.target_id);
            if (target != 0) {
                battle_target_move_cursor_to_selection(unit, target);
                frame = unit->state_frame_counter++;
                if (frame >= 0x1f) {
                    battle_map_get_tile_data_pointer(target->map_x, target->map_y, target->map_z);
                    battle_target_select_tile();
                }
            } else {
                battle_state_enter_open_action_menus();
            }
            break;
        }
    }
}

/* IllegalRange game-state handler: when the message closes (any answer), return to the range
 * display. */
void battle_state_handle_illegal_range_state(void) {
    s32 selected_command;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        battle_target_begin_tile_selection();
    }
}

/* AbilityPreviewHelp game-state handler: when the help window has gone
 * (menu_get_thread_activity returns something other than 2), return to the
 * ability preview with map panning. */
void battle_state_handle_ability_preview_help_state(void) {
    if (menu_get_thread_activity() != 2) {
        g_battle_menu_help_active = 0;
        battle_camera_enable_map_pan();
        g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING;
    }
    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* AbilityPreviewHandling game-state handler: the pending ability's targets are
 * marked and the unit confirms or backs out.
 *
 * For a player-controlled unit circle restores the cursor input mode and opens
 * the confirmation prompt (battle_state_enter_confirm_action). Cross clears the target marks
 * and returns to the idle action menu when the ability has no target choice,
 * otherwise to the range display (battle_target_begin_tile_selection). An AI-controlled
 * unit confirms by itself once its state frame counter reaches 0x1f. */
void battle_state_handle_ability_preview_state(void) {
    u16 frame;
    battle_unit_t* unit = battle_unit_get_source();

    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        battle_camera_handle_rotation_input();
        battle_camera_update_zoom_input();
        battle_camera_update_tilt_input();
        if ((g_main_input_pressed_buttons & PSX_PAD_CIRCLE) != 0) {
            g_battle_cursor_input_mode = g_battle_cursor_input_mode_saved;
            battle_gfx_restore_unit_palettes();
            battle_state_enter_confirm_action();
        } else if ((g_main_input_pressed_buttons & PSX_PAD_CROSS) != 0) {
            g_battle_cursor_input_mode = g_battle_cursor_input_mode_saved;
            battle_target_set_tile_mark_colors(BATTLE_TILE_MARK_HIDE_TARGETS, 3);
            if (unit->ability_preview_phase == TARGET_PANELS_NO_TARGETING) {
                battle_target_set_tile_mark_colors(BATTLE_TILE_MARK_NONE, 3);
                battle_gfx_restore_unit_palettes();
                battle_menu_open_active_unit_idle_action_menu();
            } else {
                battle_target_set_tile_mark_colors(BATTLE_TILE_MARK_NONE, 3);
                battle_gfx_restore_unit_palettes();
                battle_target_begin_tile_selection();
            }
        }
    } else {
        frame = unit->state_frame_counter;
        unit->state_frame_counter = frame + 1;
        if (frame >= 0x1f) {
            g_battle_cursor_input_mode = g_battle_cursor_input_mode_saved;
            battle_gfx_restore_unit_palettes();
            battle_state_enter_confirm_action();
        }
    }
    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* ConfirmAction game-state handler. The answers double as targeting types. For a player-controlled
 * unit, BATTLE_TARGETING_TYPE_TILE aims the action at the cursor tile, and BATTLE_TARGETING_TYPE_UNIT
 * or a plain YES at the unit on it (the tile when it is empty); the answer is stored as the
 * targeting type as it is, and both store the cursor tile and start the pre-attack animation. NO or
 * UNRESOLVED_FF clears the marks and returns to the range display. An AI unit starts the pre-attack
 * animation on TILE, UNIT or YES. */
void battle_state_handle_confirm_action_state(void) {
    battle_unit_t* unit;
    ai_command_t* selected_command_address;
    battle_unit_t* target;
    s32 selected_command;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    unit = battle_unit_get_source();
    if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        selected_command = selected_command_address->kind;
        switch (selected_command) {
        case BATTLE_TARGETING_TYPE_TILE:
            unit->command_state.ai.data.action.targeting_type = selected_command_address->kind;
            main_util_set_svector(
                &unit->command_state.cursor.target_panel, g_battle_cursor_x, g_battle_cursor_z, g_battle_cursor_y);
            battle_state_enter_pre_attack_animation();
            return;
        case BATTLE_TARGETING_TYPE_UNIT:
        case AI_COMMAND_PROMPT_YES:
            target = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
            if (target != 0) {
                unit->command_state.ai.data.action.targeting_type = selected_command_address->kind;
                unit->command_state.ai.data.action.target_id = target->stats->battle_id;
            } else {
                unit->command_state.ai.data.action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
            }
            main_util_set_svector(
                &unit->command_state.cursor.target_panel, g_battle_cursor_x, g_battle_cursor_z, g_battle_cursor_y);
            battle_state_enter_pre_attack_animation();
            return;
        case AI_COMMAND_PROMPT_NO:
        case AI_COMMAND_UNRESOLVED_FF:
            battle_target_set_tile_mark_colors(BATTLE_TILE_MARK_NONE, 3);
            battle_target_begin_tile_selection();
            return;
        default:
            break;
        }
    } else {
        selected_command = selected_command_address->kind;
        switch (selected_command) {
        case BATTLE_TARGETING_TYPE_TILE:
        case BATTLE_TARGETING_TYPE_UNIT:
        case AI_COMMAND_PROMPT_YES:
            battle_state_enter_pre_attack_animation();
            return;
        case AI_COMMAND_PROMPT_NO:
        case AI_COMMAND_UNRESOLVED_FF:
            battle_target_set_tile_mark_colors(BATTLE_TILE_MARK_NONE, 3);
            battle_target_begin_tile_selection();
            return;
        default:
            break;
        }
    }
}

/* PreAttackAnimation game-state handler: execute the ability once the source
 * unit's animation has ended or looped, or its state frame counter has reached
 * 0x3d, and the camera has settled. */
void battle_state_handle_pre_attack_animation_state(void) {
    battle_unit_t* unit;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    unit = battle_unit_get_source();
    if ((unit->animation.wait == 0 || unit->animation.restart_count != 0 || unit->state_frame_counter >= 0x3d)
        && (g_battle_camera_step_vector.vx | g_battle_camera_step_vector.vy | g_battle_camera_step_vector.vz) == 0
        && g_battle_camera_rotation_action == CAMERA_ROTATION_NONE) {
        battle_action_execute_ability();
    }
}

/* ActionCast game-state handler: once the message has closed and the camera has
 * settled, enter the commence-attack phase. */
void battle_state_handle_action_cast_state(void) {
    s32 selected_command;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    /* Any prompt answer marks the window closed (the same test as the
     * BATTLE_MESSAGE_DISPLAY handler). */
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        g_battle_state_window_closed = 1;
    }
    /* Wait for the camera to settle before handing the action data over. */
    if ((g_battle_camera_step_vector.vx | g_battle_camera_step_vector.vy | g_battle_camera_step_vector.vz) == 0
        && g_battle_camera_rotation_action == CAMERA_ROTATION_NONE && g_battle_state_window_closed != 0) {
        battle_state_enter_commence_attack_phase();
    }
}

/* CommenceAttackPhase game-state handler. A Jump waits for the source unit's
 * animation to end, then marks its action taken and enters the after-command
 * state. Any other action starts the pre-action phase and announces the next
 * action that is due. */
void battle_state_handle_commence_attack_phase_state(void) {
    battle_unit_t* unit;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    unit = battle_unit_get_source();
    if (unit->attack_phase_state == BATTLE_ATTACK_PREPARATION_JUMP) {
        if (unit->animation.wait == 0) {
            battle_action_set_only_action_taken(unit->stats->battle_id);
            battle_unit_end_command(unit);
        }
    } else {
        g_battle_action_phase = ACTION_PHASE_PRE_ACTION;
        battle_state_announce_next_action();
    }
}

/* ActionAnnouncement game-state handler: when the announcement message closes (any answer), enter
 * the OpenSp2Files state. */
void battle_state_handle_action_announcement_state(void) {
    s32 selected_command;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        battle_state_enter_open_sp2_files();
    }
}

/* OpenSp2Files game-state handler: load the SP2 sheet holding the casting unit's
 * attack animation, then start the strike
 * (battle_action_start_next_strike_display).
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
void battle_state_handle_open_sp2_files_state(void) {
    RECT image_rect;
    battle_unit_t* unit = battle_unit_get_casting();
    s32 open_result;
    s32 slot;
    s32 free_owner;
    s32 offset;
    RECT* image_rect_address;
    u32* sp2_data;

    if (g_battle_gfx_sp2_data == 0) {
        g_battle_gfx_sp2_data = main_heap_alloc(0x8000);
        open_result = battle_gfx_open_sp2_file(unit, g_battle_gfx_sp2_data);
        if (open_result == -1) {
            main_heap_free(g_battle_gfx_sp2_data);
            g_battle_gfx_sp2_data = 0;
            battle_action_start_next_strike_display();
        } else if (open_result == 0) {
            main_heap_free(g_battle_gfx_sp2_data);
            g_battle_gfx_sp2_data = 0;
        }
    } else if (main_file_is_still_loading() == 0) {
        slot = 0;
        free_owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
        /* Keep the target's byte-offset induction; advancing a slot pointer changes the exact code. */
        offset = 0;
        do {
            if (((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + offset))->owner == free_owner) {
                image_rect_address = &image_rect;
                sp2_data = g_battle_gfx_sp2_data;
                ((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + offset))->owner
                    = unit->runtime_id | BATTLE_GFX_VRAM_SLOT_OWNER_SP2;
                image_rect_address->x
                    = ((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + offset))->image_rect.x;
                image_rect_address->y
                    = ((battle_gfx_vram_slot_t*)((u8*)g_battle_gfx_vram_slots + offset))->image_rect.y;
                image_rect_address->w = 0x40;
                image_rect_address->h = 0x100;
                LoadImage(image_rect_address, sp2_data);
                battle_action_start_next_strike_display();
                break;
            }
            slot++;
            offset += sizeof(battle_gfx_vram_slot_t);
        } while (slot < 2);
    }
    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* StartEffectFileOpen game-state handler: step the effect-file load
 * (battle_effect_update_file_load); when no work remains, resolve the strike and pick
 * the attacker's animation. */
void battle_state_handle_start_effect_file_open_state(void) {
    if (battle_effect_update_file_load() != 0) {
        g_battle_state_animation_continue_check = 1;
    } else {
        g_battle_state_animation_continue_check = 0;
    }
    if (g_battle_state_animation_continue_check == 0) {
        battle_action_apply_strike_results();
    }
    battle_state_update_units();
}

/* ActionExecuteStart game-state handler: count the frame and enter the
 * action-execute state (battle_state_enter_action_execute). */
void battle_state_handle_action_execute_start_state(void) {
    battle_unit_t* unit = battle_unit_get_casting();

    unit->state_frame_counter++;
    battle_state_enter_action_execute();
    battle_state_update_units();
}

/* Start the target's reaction animation for the attacker's attack and prepare its post-action
 * display; nothing happens when either unit is missing. */
static void battle_unit_start_post_attack_animation_display(battle_unit_t* attacker, battle_unit_t* target) {
    if (attacker != 0 && target != 0) {
        battle_unit_set_target_animation(attacker, target);
        battle_gfx_prepare_post_action_display_by_runtime_id(target->runtime_id);
    }
}

/* ActionExecute game-state handler: animate the strike, run the knockback step of
 * every target and, when everything has settled, refresh the targets and show the
 * battle message.
 *
 * The strike has settled when no effect cycle or target step is running
 * (g_battle_state_animation_continue_check), the casting unit shows no numeric
 * display, and its animation countdown is 0 or its animation id is below
 * BATTLE_UNIT_ANIMATION_ATTACK_FIRST: only a teleport or attack animation is
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
void battle_state_update_action_execute_mode(void) {
    battle_unit_t* unit;
    battle_unit_t* target;
    s32 i;

    battle_gfx_project_all_units();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        battle_camera_update_real_coord_animation();
        battle_camera_update_screen_offset_animation();
        battle_camera_update_zoom_animation();
        battle_camera_update_rotation_animation();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    battle_target_update_cursor();

    unit = battle_unit_get_casting();
    /* The target reads the 16-bit value of g_main_system_vsync_interval. */
    unit->state_frame_counter += g_main_system_vsync_interval;
    if (unit->strike.target_count != 0) {
        for (i = 0; i < unit->strike.target_count; i++) {
            target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
            if (target != 0) {
                battle_move_update_knockback_step(target);
                g_battle_state_animation_continue_check |= target->step_phase;
            }
        }
    }

    if (unit->used_ability_id != 0 && unit->used_ability_id != ABILITY_ID_KNOCKBACK
        && unit->used_ability_id != ABILITY_ID_BASIC_SKILL_THROW_STONE) {
        if (battle_effect_ability_has_no_file(unit->used_ability_id) != 0
            || (unit->used_ability_id >= ABILITY_ID_CHARGE_FIRST && unit->used_ability_id < ABILITY_ID_MATH_FIRST)) {
            battle_camera_update_cursor_tile_vector();
        }
        if (g_battle_state_animation_continue_check == 0 && unit->numeric_display.active == 0
            && (unit->animation.wait == 0 || (unit->animation.sequence_id >> 1) < BATTLE_UNIT_ANIMATION_ATTACK_FIRST)) {
            main_sound_replay_bg_sfx();
            if (unit->strike.target_count != 0) {
                for (i = 0; i < unit->strike.target_count; i++) {
                    target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
                    if (target != 0) {
                        if (unit->used_ability_id >= ABILITY_ID_REACTION_FIRST
                            && unit->used_ability_id < ABILITY_ID_SUPPORT_FIRST) {
                            if (battle_effect_ability_has_no_file(unit->used_ability_id) != 0
                                && g_battle_ability_animation_data[unit->used_ability_id].attack_animation
                                    != BATTLE_ATTACK_ANIMATION_USE_WEAPON) {
                                battle_unit_start_post_attack_animation_display(unit, target);
                            }
                        } else {
                            battle_unit_update_display_by_runtime_id(target->runtime_id);
                        }
                    }
                }
            }
            battle_state_start_battle_message_display();
        }
    } else {
        battle_camera_update_cursor_tile_vector();
        menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
        if (g_battle_state_animation_continue_check == 0 && unit->numeric_display.active == 0
            && (unit->animation.wait == 0 || (unit->animation.sequence_id >> 1) < BATTLE_UNIT_ANIMATION_ATTACK_FIRST)) {
            if (unit->strike.target_count != 0) {
                for (i = 0; i < unit->strike.target_count; i++) {
                    target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
                    if (target != 0) {
                        if (unit->used_ability_id == ABILITY_ID_KNOCKBACK) {
                            battle_gfx_prepare_post_action_display_by_runtime_id(target->runtime_id);
                        }
                        battle_unit_update_display_by_runtime_id(target->runtime_id);
                    }
                }
            }
            battle_state_start_battle_message_display();
        }
    }
}

/* BattleMessageDisplay game-state handler: when the message closes (any answer), queue and show the
 * casting unit's post-effect messages. */
void battle_state_handle_battle_message_display_state(void) {
    s32 selected_command;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        g_battle_state_window_closed = 1;
    }
    if (g_battle_state_window_closed != 0) {
        battle_action_show_caster_post_effect_messages();
    }
}

/* ResumeAttackPhase game-state handler: show the remaining post-effect messages, then finish the
 * strike.
 *
 * Each closed message (any answer) lets battle_action_show_next_effect_message open the next queued
 * one. When none remains and the casting unit's numeric display is idle, a continued attack starts
 * its next strike. Otherwise the SP2 buffer and VRAM slot are released, the acting unit's data is
 * stored and the units return to their status animations. Outside the pre-action phase the action
 * phase then advances (up to the reacting-unit phase) and the next due action is announced; when
 * none is due, or in the pre-action phase, the JP and EXP rewards begin.
 *
 * The facing computed for the acting unit's own action is passed to
 * battle_action_post_action_facing_stub, which is empty. */
void battle_state_handle_resume_attack_phase_state(void) {
    s32 facing;
    unit_stats_t* stats;
    s32 selected_command;
    battle_unit_t* unit;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        g_battle_state_window_closed = 1;
    }
    unit = battle_unit_get_casting();
    if (g_battle_state_window_closed != 0 && battle_action_show_next_effect_message() == 0
        && unit->numeric_display.active == 0) {
        if (unit->strike.continue_attack != 0) {
            unit->continue_attack_count += 1;
            battle_action_start_next_strike_display();
            return;
        }
        if (g_battle_gfx_sp2_data != 0) {
            main_heap_free(g_battle_gfx_sp2_data);
            g_battle_gfx_sp2_data = 0;
        }
        battle_gfx_invalidate_sp2_vram_slot(unit);
        if (g_battle_action_turn_event_type == BATTLE_TURN_EVENT_UNIT_READY
            && g_battle_action_phase == ACTION_PHASE_ACTING_UNIT) {
            /* Facing in quarter turns: a signed division by 0x400 spelled out, because retail
             * finishes it with a logical shift. */
            facing = unit->facing;
            if (facing < 0) {
                facing += 0x3ff;
            }
            facing = (u32)facing >> 10;
            stats = unit->stats;
            battle_action_post_action_facing_stub(stats, facing & 0xff);
        }
        battle_action_store_acting_unit_data(unit->stats);
        battle_unit_call_set_status_animation(unit);
        battle_unit_update_target_displays(unit);
        battle_unit_get_source(); /* result unused */
        if (g_battle_action_phase != ACTION_PHASE_PRE_ACTION) {
            if (g_battle_action_phase != ACTION_PHASE_REACTING_UNIT) {
                g_battle_action_phase += 1;
            }
            if (battle_state_announce_next_action() != 0) {
                return;
            }
        }
        battle_action_grant_rewards();
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
void battle_state_handle_learn_ability_on_hit_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    s32 i;
    s32 display_active;
    s32 target_count;
    battle_unit_t* casting_unit;
    battle_unit_t* target_unit;
    u8 compiler_stack_pad[8];

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    casting_unit = battle_unit_get_casting();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case AI_COMMAND_PROMPT_YES:
        if (g_battle_action_post_action_display_phase == BATTLE_POST_ACTION_DISPLAY_LEARN_ON_HIT) {
            target_unit = battle_unit_get_by_battle_id(g_battle_action_learn_on_hit_unit_id);
            battle_unit_learn_used_ability(target_unit->stats);
        }
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
        g_battle_action_post_action_display_phase += 1;
        if (g_battle_action_post_action_display_phase >= 3) {
            g_battle_state_window_closed = 1;
        } else {
            battle_state_enter_learn_ability_on_hit();
        }
        break;
    }
    if (g_battle_state_window_closed != 0 && g_battle_state_animation_continue_check == 0) {
        i = 0;
        if (casting_unit->numeric_display.active == 0) {
            target_count = casting_unit->strike.target_count;
            display_active = 0;
            if (target_count > 0) {
                do {
                    target_unit = battle_unit_get_by_battle_id(casting_unit->strike.target_list[i]);
                    if (target_unit != 0 && target_unit->numeric_display.active != 0) {
                        display_active = 1;
                        break;
                    }
                    target_count = casting_unit->strike.target_count;
                    i += 1;
                } while (i < target_count);
            }
            if (display_active == 0) {
                casting_unit = battle_unit_get_source();
                battle_unit_call_set_status_animation(casting_unit);
                battle_unit_end_command(casting_unit);
            }
        }
    }
}

/* Frame step shared by the two Deep Dungeon mesh states: run the unit and menu update and, once
 * the map data has loaded, refresh every unit's display, clear
 * g_battle_map_deep_dungeon_load_in_progress and return 1. Returns 0 while the load continues. */
static s32 battle_map_update_deep_dungeon_and_animation(void) {
    s32 runtime_id;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    if (battle_map_poll_data_load() != 0) {
        return 0;
    }
    for (runtime_id = 0; runtime_id < BATTLE_UNIT_RUNTIME_SLOT_COUNT; runtime_id++) {
        battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);
        if (unit != 0) {
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
        }
    }
    g_battle_map_deep_dungeon_load_in_progress = 0;
    return 1;
}

/* DeepDungeonMeshLoad game-state handler: when the Deep Dungeon map has loaded,
 * run the between-turn events. */
void battle_state_handle_deep_dungeon_mesh_load_state(void) {
    if (battle_map_update_deep_dungeon_and_animation() != 0) {
        battle_action_start_next_turn_event();
    }
}

/* DeepDungeonMeshFinish game-state handler: when the Deep Dungeon map has loaded,
 * enter the after-command state. */
void battle_state_handle_deep_dungeon_mesh_finish_state(void) {
    if (battle_map_update_deep_dungeon_and_animation() != 0) {
        battle_state_enter_after_command();
    }
}

/* TargetSelectStart game-state handler: when the prompt closes (any answer), enter target
 * selection. */
void battle_state_handle_target_select_start_state(void) {
    s32 selected_command;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        battle_state_enter_target_select();
    }
}

/* TargetSelect game-state handler: the player picks the auto-battle target with the cursor. Circle
 * confirms the unit on the tile when it suits the choice (an enemy for the targeted choice, an
 * ally for the recovery choice) and denies it otherwise or on an empty tile; cross moves the
 * cursor back to the casting unit and reopens the action menus, and triangle only moves the cursor
 * back. */
void battle_state_handle_target_select_state(void) {
    battle_unit_t* caster;
    battle_unit_t* target;

    battle_camera_handle_rotation_input();
    battle_camera_update_zoom_input();
    battle_camera_update_tilt_input();
    battle_target_move_cursor_by_input();
    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    if (g_main_input_pressed_buttons & PSX_PAD_CIRCLE) {
        caster = battle_unit_get_casting();
        target = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        /* The targeted choice requires an enemy, the recovery choice an ally. */
        if (target == 0) {
            battle_state_enter_target_select_denied();
        } else if (caster->target_select_command == AI_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT) {
            if ((caster->stats->team_flags & UNIT_TEAM_MASK) == (target->stats->team_flags & UNIT_TEAM_MASK)) {
                battle_state_enter_target_select_denied();
            } else {
                battle_state_enter_target_select_confirm();
            }
        } else if (caster->target_select_command == AI_COMMAND_AUTOBATTLE_PROTECT_ALLIES_SELECT) {
            if ((caster->stats->team_flags & UNIT_TEAM_MASK) == (target->stats->team_flags & UNIT_TEAM_MASK)) {
                battle_state_enter_target_select_confirm();
            } else {
                battle_state_enter_target_select_denied();
            }
        }
        battle_gfx_restore_unit_palettes();
    } else if (g_main_input_pressed_buttons & PSX_PAD_CROSS) {
        battle_target_move_cursor_to_unit(battle_unit_get_casting());
        battle_gfx_restore_unit_palettes();
        battle_state_enter_open_action_menus();
    } else if (g_main_input_pressed_buttons & PSX_PAD_TRIANGLE) {
        battle_target_move_cursor_to_unit(battle_unit_get_casting());
    }
}

/* TargetSelectDenied game-state handler: when the message closes (any answer), return to target
 * selection. */
void battle_state_handle_target_select_denied_state(void) {
    s32 selected_command;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        battle_state_enter_target_select();
    }
}

/* TargetSelectConfirm game-state handler: YES stores the unit under the cursor as the casting
 * unit's auto-battle target and opens the next action menus; NO or UNRESOLVED_FF returns to target
 * selection. */
void battle_state_handle_target_select_confirm_state(void) {
    battle_unit_t* caster;
    battle_unit_t* target;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    switch ((s32)menu_get_selected_command_address()->kind) {
    case AI_COMMAND_PROMPT_YES:
        caster = battle_unit_get_casting();
        target = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        menu_store_auto_battle_main_target_id(target->stats->battle_id, caster->stats->battle_id);
        battle_target_move_cursor_to_unit(caster);
        battle_state_enter_open_action_menus();
        break;
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
        battle_state_enter_target_select();
        break;
    }
}

/* TargetDisplayStart game-state handler: once the prompt has closed (any answer) and the camera has
 * settled, enter the target display. */
void battle_state_handle_target_display_start_state(void) {
    s32 selected_command;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        g_battle_state_window_closed = 1;
    }
    if ((g_battle_camera_step_vector.vx | g_battle_camera_step_vector.vy | g_battle_camera_step_vector.vz) == 0) {
        if (g_battle_camera_rotation_action == CAMERA_ROTATION_NONE) {
            if (g_battle_state_window_closed != 0) {
                battle_state_enter_target_display();
            }
        }
    }
}

/* TargetDisplay game-state handler: circle or cross puts the cursor back on the
 * casting unit and opens the next action menus. */
void battle_state_handle_target_display_state(void) {
    s32 buttons;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    buttons = g_main_input_pressed_buttons;
    if ((buttons & PSX_PAD_CIRCLE) || (buttons & PSX_PAD_CROSS)) {
        battle_target_move_cursor_to_unit(battle_unit_get_casting());
        battle_state_enter_open_action_menus();
    }
}

/* AfterCommand game-state handler: once the window has closed (any answer) and the source unit's
 * numeric display is idle, enter the continue-turn state. */
void battle_state_handle_after_command_state(void) {
    ai_command_t* command_address;
    s32 command;
    battle_unit_t* unit;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    command_address = menu_get_selected_command_address();
    unit = battle_unit_get_source();
    command = command_address->kind;
    if (command >= AI_COMMAND_PROMPT_YES && (command <= AI_COMMAND_PROMPT_NO || command == AI_COMMAND_UNRESOLVED_FF)) {
        g_battle_state_window_closed = 1;
    }
    if ((g_battle_state_window_closed != 0) && (unit->numeric_display.active == 0)) {
        battle_state_enter_continue_turn();
    }
}

/* ContinueTurn game-state handler: move on once the camera has settled (no cursor
 * vector, no rotation).
 *
 * After a charged ability (turn event ABILITY_READY) a queued scenario event with
 * a nonzero finish operation starts; otherwise the between-turn events run. On a
 * unit's own turn, a unit whose turn is over (battle_action_check_change_of_turn
 * returns 1) gets its last command and ability copied back into its runtime record;
 * a Jump then resets its preparation and goes to the between-turn events, and
 * anything else opens the Wait menu. A unit whose turn goes on gets its action
 * menus back. */
void battle_state_handle_continue_turn_state(void) {
    battle_unit_t* unit;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    unit = battle_unit_get_source();
    if ((g_battle_camera_step_vector.vx | g_battle_camera_step_vector.vy | g_battle_camera_step_vector.vz) != 0) {
        return;
    }
    if (g_battle_camera_rotation_action != CAMERA_ROTATION_NONE) {
        return;
    }
    if (g_battle_action_turn_event_type == BATTLE_TURN_EVENT_ABILITY_READY) {
        if (menu_init_system_function(
                MENU_SYSTEM_COMMAND_SCENARIO_CHECK, MENU_SCENARIO_CHECK_TEST, unit->stats->battle_id, 0, 1)
                == MENU_SYSTEM_DISPATCH_EVENT
            && script_get_event_finish_operation() != 0) {
            g_battle_previous_game_state = g_battle_game_state;
            menu_init_system_function(
                MENU_SYSTEM_COMMAND_SCENARIO_CHECK, MENU_SCENARIO_CHECK_START, unit->stats->battle_id, 0, 1);
            battle_state_enter_event();
            return;
        }
        battle_action_start_next_turn_event();
        return;
    }
    if (battle_action_check_change_of_turn(unit->stats->battle_id) == 1) {
        /* The stats keep a copy of the command the unit acted on. */
        unit->command_state.ai.data.action = unit->stats->action_command;
        unit->used_ability_id = unit->stats->action_command.ability_id;
        if (unit->attack_phase_state == BATTLE_ATTACK_PREPARATION_JUMP) {
            unit->attack_phase_state = BATTLE_ATTACK_PREPARATION_CHARGE;
            battle_gfx_hide_unit_by_runtime_id(unit->runtime_id);
            battle_action_start_next_turn_event();
            return;
        }
        battle_state_enter_wait_menu();
        return;
    }
    battle_state_enter_open_action_menus();
}

/* ChangeTurn game-state handler. Once the menu layer reports an answer (the message closed) and the
 * camera has settled (no cursor vector, no rotation), the turn event type picks the next state: a
 * ready ability enters the action cast, an action result or a Mime action enters status execution,
 * and anything else opens the next action menus. */
void battle_state_handle_change_turn_state(void) {
    s32 command;
    s32 type;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    command = menu_get_selected_command_address()->kind;
    if (command >= AI_COMMAND_PROMPT_YES) {
        if (command <= AI_COMMAND_PROMPT_NO) {
            g_battle_state_window_closed = 1;
        } else if (command == AI_COMMAND_UNRESOLVED_FF) {
            g_battle_state_window_closed = 1;
        }
    }
    if (g_battle_state_window_closed != 0) {
        if ((g_battle_camera_step_vector.vx | g_battle_camera_step_vector.vy | g_battle_camera_step_vector.vz) == 0) {
            if (g_battle_camera_rotation_action == CAMERA_ROTATION_NONE) {
                battle_unit_get_source(); /* result unused */
                type = g_battle_action_turn_event_type;
                if (type == BATTLE_TURN_EVENT_ABILITY_READY) {
                    battle_state_enter_action_cast();
                    return;
                }
                if ((type == BATTLE_TURN_EVENT_ACTION_RESULT) || (type == BATTLE_TURN_EVENT_MIME)) {
                    battle_state_enter_status_execute();
                    return;
                }
                battle_state_enter_open_action_menus();
            }
        }
    }
}

/* StatusExecute game-state handler: once the message has closed (any answer), no animation is
 * pending and the source unit's numeric display is idle, refresh the unit's display and run the
 * between-turn events. */
void battle_state_handle_status_execute_state(void) {
    s32 selected_command;
    battle_unit_t* unit;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        g_battle_state_window_closed = 1;
    }
    if (g_battle_state_window_closed != 0) {
        if (g_battle_state_animation_continue_check == 0) {
            unit = battle_unit_get_source();
            if (unit->numeric_display.active == 0) {
                battle_unit_update_display_by_runtime_id(unit->runtime_id);
                battle_action_start_next_turn_event();
            }
        }
    }
}

/* Effect game-state handler: animate the units, the screen color modulation and
 * the camera while an effect plays. When the effect cycle has ended, the game
 * state and vsync interval saved by battle_state_enter_effect_playback are
 * restored. Neither function that stores this state has a caller, so the
 * handler does not run. */
void battle_state_handle_effect_state(void) {
    battle_gfx_project_all_units();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        battle_camera_update_real_coord_animation();
        battle_camera_update_screen_offset_animation();
        battle_camera_update_zoom_animation();
        battle_camera_update_rotation_animation();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    if (g_battle_state_animation_continue_check == 0) {
        g_battle_game_state = g_battle_previous_game_state;
        g_main_system_vsync_interval = g_main_saved_vsync_interval;
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
void battle_state_update_event_mode(void) {
    s16 intensity;
    u16 level;
    char unused[24];

    battle_gfx_project_all_units();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        battle_camera_update_real_coord_animation();
        battle_camera_update_screen_offset_animation();
        battle_camera_update_zoom_animation();
        battle_camera_update_rotation_animation();
        battle_move_update_all_unit_paths();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    switch (script_run_event_frame(main_gfx_get_otag(), g_input_current_buttons)) {
    case SCRIPT_EVENT_FRAME_NO_FINISH:
        battle_gfx_init_evtchr_vram_slots();
        g_battle_menu_status_enabled = 1;
        battle_ai_init_workspace();
        battle_action_start_next_turn_event();
        break;
    case SCRIPT_EVENT_FRAME_TO_WORLD:
        battle_gfx_init_evtchr_vram_slots();
        battle_state_enter_close_battle(8, MAIN_GAME_FLOW_WORLD);
        break;
    case SCRIPT_EVENT_FRAME_NEXT_EVENT:
        battle_gfx_init_evtchr_vram_slots();
        battle_state_enter_close_battle(8, MAIN_GAME_FLOW_BATTLE);
        break;
    case SCRIPT_EVENT_FRAME_RESET:
        battle_gfx_init_evtchr_vram_slots();
        battle_state_enter_close_battle(8, MAIN_GAME_FLOW_RESET);
        break;
    case SCRIPT_EVENT_FRAME_ENDING:
        battle_gfx_init_evtchr_vram_slots();
        battle_state_enter_close_battle(8, MAIN_GAME_FLOW_ENDING);
        break;
    }

    if (g_battle_screen_fade_intensity != 0) {
        intensity = g_battle_screen_fade_intensity;
        level = intensity;
        if (level >= 0x100) {
            level = 0xff;
        }
        g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = level;
        g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = level;
        g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = level;

        AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
        AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
    }
}

/* Stop the map's palette cycles and weather sound; every path that leaves a map calls it just
 * before battle_unit_clear_all. */
void battle_state_stop_map_animations(void) {
    battle_map_stop_clut_cycles_and_weather_sfx();
}

/* MapJumpingOut game-state handler: darken the screen by
 * g_battle_state_screen_fade_step per frame. Past full black it stops the map
 * animations, clears the units and starts the map init sequence (MapInitialize),
 * which fades back in by itself. */
void battle_state_handle_map_jumping_out_state(void) {
    u16 intensity;
    char unused[24];

    battle_state_update_units();
    intensity = g_battle_screen_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }

    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = intensity;

    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);

    g_battle_screen_fade_intensity += g_battle_state_screen_fade_step;
    if (g_battle_screen_fade_intensity >= 0x102) {
        g_battle_screen_fade_intensity = 0xff;
        g_battle_game_state = BATTLE_GAME_STATE_MAP_INITIALIZE;
        g_battle_state_map_init_step = 0;
        battle_state_stop_map_animations();
        battle_unit_clear_all();
    }
}

/* MapJumpingOut2 game-state handler: darken the screen by
 * g_battle_state_screen_fade_step per frame. Past full black it stops the map
 * animations, clears the units and starts the map init sequence (MapInitialize2). */
void battle_state_handle_change_map_jumping_out_state(void) {
    u16 intensity;
    char unused[24];

    battle_state_update_units();
    intensity = g_battle_screen_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }

    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = intensity;

    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);

    g_battle_screen_fade_intensity += g_battle_state_screen_fade_step;
    if (g_battle_screen_fade_intensity >= 0x102) {
        g_battle_screen_fade_intensity = 0xff;
        g_battle_game_state = BATTLE_GAME_STATE_MAP_INITIALIZE_2;
        g_battle_state_map_init_step = 0;
        battle_state_stop_map_animations();
        battle_unit_clear_all();
    }
}

/* CloseBattle game-state handler: draw the fade overlay and raise its intensity
 * by g_battle_state_screen_fade_step. The game loop leaves in the frame it finds
 * this state, so the handler runs for at most one frame. */
void battle_state_handle_close_battle_state(void) {
    u16 intensity;
    /* The original frame reserves this otherwise-unused stack area. */
    u8 stack_padding[0x18];

    battle_state_update_units();
    intensity = g_battle_screen_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = intensity;
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
    g_battle_screen_fade_intensity += g_battle_state_screen_fade_step;
}

/* MapInitialize game-state handler: run one step of the map init sequence behind
 * a black screen; at step 0xd start the fade-in (MapJumpingIn). */
void battle_state_handle_map_init_state(void) {
    char unused[24]; /* unreferenced; sizes the target's 0x30-byte frame */

    g_battle_state_map_init_step = battle_map_step_init_sequence(g_battle_map_id, g_battle_state_map_init_step);
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = 0xff;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = 0xff;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = 0xff;
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
    if (g_battle_state_map_init_step == 0xd) {
        g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_IN;
        g_battle_screen_fade_intensity = 0xff;
    }
}

/* MapInitialize2 game-state handler, the map change an event asks for: run one
 * step of the map init sequence behind a black screen. At step 0xd the saved game
 * state resumes with the screen still black; the event fades it in with
 * battle_state_start_change_map_jump_in. */
void battle_state_handle_change_map_init_state(void) {
    s32 old_state;
    char unused[24]; /* unreferenced; sizes the target's 0x30-byte frame */

    g_battle_state_map_init_step = battle_map_step_init_sequence(g_battle_map_id, g_battle_state_map_init_step);
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = 0xff;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = 0xff;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = 0xff;
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
    if (g_battle_state_map_init_step == 0xd) {
        old_state = g_battle_previous_game_state;
        g_battle_previous_game_state = BATTLE_GAME_STATE_FREE_CURSOR;
        g_battle_screen_fade_intensity = 0xff;
        g_battle_game_state = old_state;
    }
}

/* MapJumpingIn game-state handler: lighten the screen by
 * g_battle_state_screen_fade_step per frame. When it is clear, the saved game
 * state resumes at vsync interval 1. */
void battle_state_handle_map_jumping_in_state(void) {
    u16 intensity;
    s32 previous_state;
    char unused[24];

    battle_state_update_units();
    intensity = g_battle_screen_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = intensity;
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);
    g_battle_screen_fade_intensity -= g_battle_state_screen_fade_step;
    if (g_battle_screen_fade_intensity <= 0) {
        previous_state = g_battle_previous_game_state;
        g_battle_screen_fade_intensity = 0;
        g_battle_previous_game_state = BATTLE_GAME_STATE_FREE_CURSOR;
        g_main_system_vsync_interval = 1;
        g_battle_game_state = previous_state;
    }
}

/* MapJumpingIn2 game-state handler, the fade-in that ends an event's map change:
 * it advances the simulation unless the status screen owns the frame, runs the
 * event script and then draws the subtractive fade overlay one step lighter. When
 * the fade reaches zero the saved game state is restored. */
void battle_state_handle_change_map_jumping_in_state(void) {
    u16 intensity;
    char unused[24];

    battle_gfx_project_all_units();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
        battle_gfx_update_screen_color_modulation_fade();
        battle_camera_update_real_coord_animation();
        battle_camera_update_screen_offset_animation();
        battle_camera_update_zoom_animation();
        battle_camera_update_rotation_animation();
        battle_move_update_all_unit_paths();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_gfx_draw_screen_color_modulation_overlay();
    /* TO_WORLD and NEXT_EVENT lead the other way round from battle_state_update_event_mode
     * (QUIRKS.md). */
    switch (script_run_event_frame(main_gfx_get_otag(), g_input_current_buttons)) {
    case SCRIPT_EVENT_FRAME_RUNNING:
        break;
    case SCRIPT_EVENT_FRAME_NO_FINISH:
        battle_action_start_next_turn_event();
        break;
    case SCRIPT_EVENT_FRAME_TO_WORLD:
        battle_state_enter_close_battle(8, MAIN_GAME_FLOW_BATTLE);
        break;
    case SCRIPT_EVENT_FRAME_NEXT_EVENT:
        battle_state_enter_close_battle(8, MAIN_GAME_FLOW_WORLD);
        break;
    }

    intensity = g_battle_screen_fade_intensity;
    if (intensity >= 0x100) {
        intensity = 0xff;
    }
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].r0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].g0 = intensity;
    g_battle_screen_fade_polygons[g_main_gfx_screen_polarity].b0 = intensity;

    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_polygons[g_main_gfx_screen_polarity]);
    AddPrim(main_gfx_get_otag(), &g_battle_screen_fade_draw_modes[g_main_gfx_screen_polarity]);

    g_battle_screen_fade_intensity -= g_battle_state_screen_fade_step;
    if (g_battle_screen_fade_intensity <= 0) {
        g_battle_screen_fade_intensity = 0;
        g_battle_game_state = g_battle_previous_game_state;
        g_battle_previous_game_state = BATTLE_GAME_STATE_FREE_CURSOR;
    }
}
