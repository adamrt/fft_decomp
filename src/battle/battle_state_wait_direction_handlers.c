/* Wait-direction, crystal-learn and action-execute-setup handlers. */

#include "fft/battle.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/pad.h"

/* Stores a unit's tile and facing in its battle stats and gives its mount partner
 * the same facing, camera-relative facing and tile. Run when the wait direction
 * is confirmed. */
void battle_unit_set_tile_position_with_mount_partner(battle_unit_t* unit) {
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
 * Ending the turn applies action_end_turn and re-registers the unit's tile
 * together with its mount partner. */
void battle_state_handle_wait_direction_state(void) {
    s32 facing;
    s32 facing_hint;
    u16 frame_data;
    battle_unit_t* unit;
    battle_unit_t* mounted_unit;

    unit = battle_unit_get_source();
    battle_gfx_update_all_unit_rotation_and_vectors();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
    }
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        battle_gfx_draw_wait_direction_unit();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        camera_handle_rotation_input();
        camera_update_zoom_input();
        camera_update_tilt_input();
        if ((g_main_input_pressed_buttons & PSX_PAD_CIRCLE) != 0) {
            if (unit->stats != 0) {
                action_end_turn(unit->stats->battle_id);
                battle_unit_set_tile_position_with_mount_partner(unit);
            }
            main_sound_play_sfx(MAIN_SFX_CONFIRM);
            action_check_between_turn_events();
        } else if ((g_main_input_pressed_buttons & PSX_PAD_CROSS) != 0) {
            battle_state_enter_after_command();
        }
        /* The target passes only the mode; the definition's second parameter is a matching device. */
        facing = camera_get_input_direction(CAMERA_INPUT_PRESSED);
        if (facing != -1 && facing != g_battle_state_wait_direction_facing) {
            main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
            g_battle_state_wait_direction_facing = facing;
        }
        battle_unit_decide_facing_direction(unit, facing);
        if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
            mounted_unit = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
            if (mounted_unit != 0) {
                battle_unit_decide_facing_direction(mounted_unit, facing);
            }
        }
    } else {
        facing_hint = unit->command_state.cursor.facing_hint;
        if (facing_hint < 4) {
            battle_unit_decide_facing_direction(unit, facing_hint << 10);
            if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
                mounted_unit = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                if (mounted_unit != 0) {
                    battle_unit_decide_facing_direction(mounted_unit, unit->command_state.cursor.facing_hint << 10);
                }
            }
        }
        frame_data = unit->state_frame_counter++;
        if (frame_data >= 0x1f) {
            if (unit->stats != 0) {
                action_end_turn(unit->stats->battle_id);
                battle_unit_set_tile_position_with_mount_partner(unit);
            }
            action_check_between_turn_events();
        }
    }
}

/*
 * Post-action state tick: apply the selected menu command, then once the
 * caster's wait counter passes 0x1f, retire one pending move-find result.
 */
void battle_state_handle_crystal_learn_state(void) {
    ai_command_t* command;
    battle_unit_t* source;
    battle_unit_t* casting;
    u16 counter;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    command = menu_get_selected_command_address();
    source = battle_unit_get_source();
    casting = battle_unit_get_casting();
    switch ((s32)command->kind) {
    case 2:
        battle_unit_learn_from_crystal(source->stats, BATTLE_CRYSTAL_RESTORE_HP_MP);
        g_action_post_action = 1;
        action_finalize_attack_and_flag_reactions(source->stats->battle_id);
        battle_gfx_prepare_post_action_display(source);
        battle_unit_update_display_by_runtime_id(source->runtime_id);
        break;
    case 0:
    case 1:
    case 4:
        battle_unit_learn_from_crystal(source->stats, command->kind);
        g_action_post_action = 1;
        break;
    case 7:
    case 8:
    case 0xff:
        g_action_post_action = 1;
        break;
    }

    if (g_action_post_action == 0) {
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

    if (g_move_find_result & MOVE_POST_EVENT_CRYSTAL_OR_TREASURE) {
        if (battle_unit_get_crystal_or_treasure_at_map_coords(source->map_x, source->map_y, source->map_z) != 0) {
            return;
        }
        g_move_find_result &= ~MOVE_POST_EVENT_CRYSTAL_OR_TREASURE;
        ai_init_selected_action();
    } else if (g_move_find_result & MOVE_POST_EVENT_MOVEMENT_BENEFIT) {
        g_action_post_action_display_phase += 1;
        if (g_action_post_action_display_phase >= 2) {
            g_move_find_result &= ~MOVE_POST_EVENT_MOVEMENT_BENEFIT;
        }
    } else if (g_move_find_result & MOVE_POST_EVENT_ITEM_FOUND) {
        if ((source->animation.wait != 0) && ((source->animation.sequence_id >> 1) >= 0xc)) {
            return;
        }
        source->item_display_active = 0;
        g_move_find_result &= ~MOVE_POST_EVENT_ITEM_FOUND;
    } else if (g_move_find_result & MOVE_POST_EVENT_TRAP_TRIGGERED) {
        g_move_find_result &= ~MOVE_POST_EVENT_TRAP_TRIGGERED;
        ai_init_selected_action();
        battle_unit_update_display_by_runtime_id(source->runtime_id);
    } else if (g_move_find_result & MOVE_POST_EVENT_CHARGING_CANCEL) {
        g_move_find_result &= ~MOVE_POST_EVENT_CHARGING_CANCEL;
    } else if (g_move_find_result & MOVE_POST_EVENT_SOURCE_DISPLAY) {
        g_move_find_result &= ~MOVE_POST_EVENT_SOURCE_DISPLAY;
    } else if (g_move_find_result & MOVE_POST_EVENT_MOUNT_STATUS_CHANGE) {
        g_move_find_result &= ~MOVE_POST_EVENT_MOUNT_STATUS_CHANGE;
    } else {
        return;
    }
    move_start_next_post_movement_step();
}

/* ActionExecuteSetup game-state handler: command 7 goes on to the range display
 * (target_set_boxes_red); 8 or 0xff returns to the idle action menu. */
void battle_state_handle_action_execute_setup_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    switch (selected_command) {
    case 7:
        target_set_boxes_red();
        return;
    case 8:
    case 0xff:
        battle_menu_open_active_unit_idle_action_menu();
        return;
    }
}
