#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/main.h"
#include "fft/unit.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_state_enter_after_command(void) {
    g_main_system_animation_speed = 1;
    g_battle_game_state = BATTLE_GAME_STATE_AFTER_COMMAND;
    g_action_post_action = 0;
    target_move_cursor_to_unit(battle_unit_get_source());
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
s32 action_report_level_up(battle_unit_t* unit) {
    if (unit->strike.action_rewards.level_for_display != 0) {
        u8 battle_id;
        text_set_message_duration_frames(0xB4);
        battle_id = unit->stats->battle_id;
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x183F, battle_id, battle_id, 1);
        battle_unit_set_level_up_animation(unit);
        target_move_cursor_to_unit(unit);
        effect_set_secondary_level_up(unit);
        main_sound_play_tune(MAIN_SOUND_TUNE_LEVEL_UP);
        return 1;
    }
    return 0;
}

/* Report a job level up recorded in the unit's rewards: message 0x183e for three
 * seconds, the level-up animation and effect, the cursor on the unit and the job
 * level-up tune. Returns 1 after reporting one, otherwise 0. */
s32 action_report_job_level_up(battle_unit_t* unit) {
    if (unit->strike.action_rewards.job_level_for_display != 0) {
        u8 battle_id;
        text_set_message_duration_frames(0xB4);
        battle_id = unit->stats->battle_id;
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x183E, battle_id, battle_id, 1);
        battle_unit_set_level_up_animation(unit);
        target_move_cursor_to_unit(unit);
        effect_set_secondary_level_up(unit);
        main_sound_play_tune(MAIN_SOUND_TUNE_JOB_LEVEL_UP);
        return 1;
    }
    return 0;
}

/* Starts the next post-action step selected by g_move_find_result.
 *
 * Handles, in priority order, the crystal/treasure pickup (bit 0x1), the
 * EXP/JP and level-up reports (0x2), a found rare/common item (0x4), a tile
 * trap (0x8), a canceled charge (0x10) and the post-action displays for the
 * source (0x200) or its mount partner (0x400). Returns 1 when a step was
 * started, otherwise refreshes both units, re-enters the after-command state
 * unless the Deep Dungeon map refresh takes over, and returns 0. */
s32 move_start_next_post_movement_step(void) {
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
    g_action_post_action = 0;
    casting->state_frame_counter = 0;
    if (g_move_find_result & MOVE_POST_EVENT_CRYSTAL_OR_TREASURE) {
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
                action_finalize_attack_and_flag_reactions(source->stats->battle_id);
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
    if (g_move_find_result & MOVE_POST_EVENT_MOVEMENT_BENEFIT) {
        switch (g_action_post_action_display_phase) {
        case BATTLE_POST_ACTION_DISPLAY_LEVEL_UP:
            action_init_movement_ability_benefit(casting->stats);
            battle_unit_build_gained_exp_jp_level_job_level(casting->stats, &casting->strike.action_rewards);
            battle_gfx_prepare_post_action_display(casting);
            battle_gfx_init_earned_exp_jp_display(casting);
            action_report_level_up(casting);
            return 1;
        case BATTLE_POST_ACTION_DISPLAY_JOB_LEVEL_UP:
            action_report_job_level_up(casting);
            return 1;
        }
        return 1;
    }
    if (g_move_find_result & MOVE_POST_EVENT_ITEM_FOUND) {
        g_move_find_display_item = *map_determine_rare_common_item(casting->stats);
        battle_gfx_store_item_display_data(source, g_move_find_display_item.item_id);
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_FOUND_ITEM, source->facing, source);
        if (!(source->team_flags & BATTLE_TEAM_MASK)) {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FIND_ITEM, g_move_find_display_item.item_id,
                source->stats->battle_id, 0, 1);
            main_sound_play_tune(MAIN_SOUND_TUNE_ITEM_FOUND);
            return 1;
        }
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FIND_ITEM, g_move_find_display_item.item_id,
            source->stats->battle_id, 0, 0);
        return 1;
    }
    if (g_move_find_result & MOVE_POST_EVENT_TRAP_TRIGGERED) {
        trap = action_apply_tile_trap(casting->stats);
        if ((source->pending_attack_result = action_finalize_attack_and_flag_reactions(source->stats->battle_id))
            == -1) {
            battle_unit_find_relocation_tile(source->stats->battle_id, &source->dismount);
            battle_unit_set_map_coords_after_death_dismount(source);
        }
        battle_gfx_prepare_post_action_display(source);
        battle_unit_set_target_animation_from_attack_type(0, source);
        effect_set_secondary_venom_trap(source);
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TRAP_RESULT, trap, source->stats->battle_id, 0, 1);
        ai_record_triggered_trap_coords(casting->map_x, casting->map_z, casting->map_y);
        return 1;
    }
    if (g_move_find_result & MOVE_POST_EVENT_CHARGING_CANCEL) {
        status_remove_charging_ability_ct(casting->stats, 1);
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
        return 1;
    }
    if (g_move_find_result & MOVE_POST_EVENT_SOURCE_DISPLAY) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE, 0, source->stats->battle_id, 0, 0);
        battle_gfx_prepare_post_action_display_by_runtime_id(source->runtime_id);
        battle_unit_update_display_by_runtime_id(source->runtime_id);
        return 1;
    }
    if (g_move_find_result & MOVE_POST_EVENT_MOUNT_STATUS_CHANGE) {
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
    if (map_refresh_deep_dungeon() == 0) {
        battle_state_enter_after_command();
    }
    return 0;
}

/* Stop the game flow after the casting unit's move and start the post-movement
 * steps.
 *
 * move_set_target_for_mounted_unit_and_find_item builds the step flags in
 * g_move_find_result (0 for a unit without battle data), and the
 * post-action display starts at the level-up report. When any step is pending the
 * map cursor and its tile glow are hidden. */
void move_init_post_movement_display(void) {
    unit_stats_t* stats;

    camera_disable_map_pan();
    stats = battle_unit_get_casting()->stats;
    if (stats != 0) {
        g_move_find_result = move_set_target_for_mounted_unit_and_find_item(stats);
    } else {
        g_move_find_result = 0;
    }
    g_action_post_action_display_phase = BATTLE_POST_ACTION_DISPLAY_LEVEL_UP;
    move_start_next_post_movement_step();
    if (g_move_find_result != 0) {
        action_clear_at_list_active();
    }
    g_battle_state_animation_continue_check = 0;
}

void action_choose_facing_for_wait(void) {
    battle_unit_t* mounted_unit;
    battle_unit_t* unit;
    s32 facing;

    camera_disable_map_pan();
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
        unit->command_state.cursor.facing_hint = ai_choose_wait_facing();
    }
}

/* Open the Wait menu for the source unit, or skip it.
 *
 * A scenario check that finds a pending event whose finish operation is nonzero
 * starts that event instead. A unit whose turn status blocks the Wait menu goes
 * straight to the between-turn events. Otherwise the wait-menu state opens with
 * the cursor unit's panel. The AT-list-active flag follows whether the player
 * controls the unit. */
void action_choose_wait(void) {
    battle_unit_t* unit;

    camera_disable_map_pan();
    unit = battle_unit_get_source();
    if (menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 2, unit->stats->battle_id, 0, 1) == 2
        && script_get_event_finish_operation() != 0) {
        g_battle_previous_game_state = g_battle_game_state;
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, unit->stats->battle_id, 0, 1);
        action_set_casting_unit_id_ff();
        return;
    }
    if (status_calculate_turn_flags(unit->stats) & BATTLE_TURN_STATUS_BLOCKS_WAIT_MENU_MASK) {
        action_check_between_turn_events();
    } else {
        g_battle_game_state = BATTLE_GAME_STATE_WAIT_MENU;
        target_store_cursor_unit_name_and_data();
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_WAIT_MENU, 0, unit->stats->battle_id, 0,
            unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    }
    if (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
        action_set_at_list_active();
    } else {
        action_clear_at_list_active();
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
void battle_unit_save_facing_before_attack(battle_unit_t* unit) {
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
void battle_unit_restore_facing_after_attack(battle_unit_t* unit) {
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
void battle_unit_restore_all_facings_after_attack(void) {
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
    target_move_cursor_to_unit(unit);
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

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ACTION_EXECUTE_SETUP;
    unit = battle_unit_get_source();
    if (unit->ability_preview_phase == TARGET_PANELS_NONE)
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 1, unit->stats->battle_id, 0,
            unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    else
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 0, unit->stats->battle_id, 0,
            unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    if (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)
        action_set_at_list_active();
    else
        action_clear_at_list_active();
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
void action_set_item_throw_stone_ability_display(void) {
    battle_unit_t* unit;
    s32 ability;

    g_battle_game_state = BATTLE_GAME_STATE_ACTION_EXECUTE;
    unit = battle_unit_get_casting();
    ability = unit->used_ability_id;
    unit->state_frame_counter = 0;
    target_move_cursor_to_action_target(unit);

    if ((ability == 0) || (ability == ABILITY_ID_KNOCKBACK)) {
        g_main_system_animation_speed = 1;
        action_clear_at_list_active();
        return;
    }
    if (effect_load_ability(ability) != 0) {
        g_main_system_animation_speed = 1;
        action_clear_at_list_active();
        return;
    }
    if ((ability == ABILITY_ID_BASIC_SKILL_ACCUMULATE)
        || (ability >= ABILITY_ID_CHARGE_FIRST && ability < ABILITY_ID_MATH_FIRST)) {
        if (effect_is_item_ability(ability) != 0) {
            g_main_system_animation_speed = 1;
            action_clear_at_list_active();
            return;
        }
        effect_play();
        g_main_system_animation_speed = 1;
        action_clear_at_list_active();
        return;
    }
    if ((effect_is_item_ability(ability) == 0)
        || ((unit->animation.sequence_id >> 1) == BATTLE_UNIT_ANIMATION_HOLD_ITEM)) {
        effect_play();
    }
    main_sound_stop_current_weather_sfx();
    g_main_system_animation_speed = 2;
    action_clear_at_list_active();
}

/* Enter the resume-attack-phase state and show the last queued post-effect
 * message, if any, as a post-action result. Returns 1 after showing a message and
 * 0 when the queue is empty. */
s32 action_resume_attack_phase_control(void) {
    s32 idx;

    g_battle_game_state = BATTLE_GAME_STATE_RESUME_ATTACK_PHASE;
    g_main_system_animation_speed = 1;
    battle_unit_get_casting();
    idx = g_action_post_effect_msg_counter;
    if (idx != 0) {
        idx -= 1;
        g_action_post_effect_msg_counter = idx;
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_POST_ACTION_RESULT, g_action_post_effect_msgs[idx].code,
            g_action_post_effect_msgs[idx].unit, g_action_post_effect_msgs[idx].value, 1);
        g_action_post_action = 0;
        return 1;
    }
    return 0;
}

/*
 * Queue the post-action "effect message" popups for one unit.
 *
 * Walks the 11 reportable outcomes recorded in the unit's
 * action_data_t and appends one 8-byte record per outcome to the
 * queue at g_action_post_effect_msgs: message code at +0, the acting unit's runtime ID at +1,
 * and the reported value at +4. g_action_post_effect_msg_counter is
 * the record count, drained by action_resume_attack_phase_control.
 *
 * The 11-way dispatch really is a jump table in the target (at 0x80067548),
 * which is why the .ld places a .rodata output section there.
 */
void action_queue_post_effect_messages_for_unit(battle_unit_t* runtime) {
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
    if (g_action_post_effect_msg_counter >= 0x10) {
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
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].code = 0x28;
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].value = action->sp_change;
                queued = 1;
            }
            break;
        case 1:
            if (action->ct_change != 0) {
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].code = 0x27;
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].value = action->ct_change;
                queued = 1;
            }
            break;
        case 2:
            if (action->brave_change != 0) {
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].code = 0x29;
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].value = action->brave_change;
                queued = 1;
            }
            break;
        case 3:
            if (action->faith_change != 0) {
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].code = 0x2a;
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].value = action->faith_change;
                queued = 1;
            }
            break;
        case 4:
            if (action->pa_change != 0) {
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].code = 0x2b;
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].value = action->pa_change;
                queued = 1;
            }
            break;
        case 5:
            if (action->ma_change != 0) {
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].code = 0x2c;
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].value = action->ma_change;
                queued = 1;
            }
            break;
        case 6:
            if (action->gil_change > 0) {
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].code = 0x2e;
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].value = action->gil_change;
                queued = 1;
            }
            break;
        case 7:
            if (action->exp_change & 0x80) {
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].code = 0x2f;
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].value = action->exp_change;
                queued = 1;
            }
            break;
        case 8:
            if (action->special_effect & (ACTION_SPECIAL_EFFECT_LEVEL_UP | ACTION_SPECIAL_EFFECT_LEVEL_DOWN)) {
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].code = 0x2d;
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].value = action->special_effect;
                queued = 1;
            }
            break;
        case 9:
            if (action->special_effect
                & (ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN)) {
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].code = 0x1d;
                off = g_action_post_effect_msg_counter;
                g_action_post_effect_msgs[off].value = action->item_lost;
                if (action->special_effect & ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN) {
                    g_action_post_effect_msgs[off].value = action->item_lost | 0x8000;
                }
                queued = 1;
            }
            break;
        case 10:
            if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                queued = 1;
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].code = 0x1c;
                g_action_post_effect_msgs[g_action_post_effect_msg_counter].value = action->item_lost;
            }
            break;
        }
        if (queued != 0) {
            g_action_post_effect_msgs[g_action_post_effect_msg_counter].unit = runtime->stats->battle_id;
            g_action_post_effect_msg_counter += 1;
        }
    }
}

/* Empty the post-effect message queue, then queue the messages of every target of
 * the casting unit. */
void action_build_targets_post_action_message(void) {
    battle_unit_t* base;
    s32 i;

    g_action_post_effect_msg_counter = 0;
    base = battle_unit_get_casting();
    for (i = 0; i < base->strike.target_count; i++) {
        action_queue_post_effect_messages_for_unit(battle_unit_get_by_battle_id(base->strike.target_list[i]));
    }
}

/* Queue the casting unit's own post-effect messages and show the first one. With
 * effect messages turned off, or nothing to report, an effect message with text
 * id 0 is requested instead. */
void action_show_caster_post_effect_messages(void) {
    battle_unit_t* unit;

    unit = battle_unit_get_casting();
    action_queue_post_effect_messages_for_unit(unit);
    if (g_main_game_options.fields.effect_messages != GAME_OPTION_ON) {
        g_action_post_effect_msg_counter = 0;
    }
    if (action_resume_attack_phase_control() == 0) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
}

/* Applies the prepared action result to a unit and shows it.
 *
 * The result code is kept in pending_attack_result; -1 (a rider killed on its
 * mount) finds a relocation tile and moves the unit there. The post-action
 * display and the display update follow. */
void battle_unit_update_attack_result_animation(battle_unit_t* unit) {
    unit->pending_attack_result = action_store_target_stats_pointer_data(unit->stats->battle_id);
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
    g_action_post_action = 0;
    unit = battle_unit_get_casting();
    action_build_targets_post_action_message();
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
void action_init_learn_ability_on_hit(void) {
    battle_unit_t* unit;
    battle_unit_t* learner;
    u16 ability_id;

    g_battle_game_state = BATTLE_GAME_STATE_LEARN_ABILITY_ON_HIT;
    g_main_system_animation_speed = 1;
    unit = battle_unit_get_source();
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    text_set_message_duration_frames(0x1E);
    switch (g_action_post_action_display_phase) {
    case BATTLE_POST_ACTION_DISPLAY_LEVEL_UP:
        action_report_level_up(unit);
        break;
    case BATTLE_POST_ACTION_DISPLAY_JOB_LEVEL_UP:
        action_report_job_level_up(unit);
        break;
    case BATTLE_POST_ACTION_DISPLAY_LEARN_ON_HIT:
        g_action_learn_on_hit_unit_id = battle_ability_find_learn_on_hit_unit(unit->stats->battle_id, &ability_id);
        if (g_action_learn_on_hit_unit_id != -1) {
            learner = battle_unit_get_by_battle_id(g_action_learn_on_hit_unit_id);
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_ABILITY_LEARNED, ability_id, unit->stats->battle_id, 0,
                learner->stats->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
            target_move_cursor_to_unit(learner);
            action_set_at_list_active();
        } else {
            g_action_post_action_display_phase++;
        }
        break;
    }
    target_store_cursor_unit_name_and_data();
}

/* Start the post-action reports: restore the units' facings, apply the source
 * unit's EXP and JP rewards (clearing the display values when nothing is
 * awarded), start the earned EXP/JP display and enter the level-up phase of
 * action_init_learn_ability_on_hit. */
void action_handle_post_action_xp_jp_ability(void) {
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
    g_action_post_action_display_phase = BATTLE_POST_ACTION_DISPLAY_LEVEL_UP;
    target_move_cursor_to_unit(unit);
    action_init_learn_ability_on_hit();
    g_action_post_action = 0;
    g_battle_state_animation_continue_check = 0;
}

/* Build the secondary-effect init record for an ability and hand it to the
 * effect selector.
 *
 * Each of the acting unit's targets contributes a target block carrying its
 * unit id and a result animation; with no targets the first block instead
 * holds the post-action destination tile. Every other living unit that is not
 * already a target is appended to spectator_ids. The packed
 * argument combines the ability's charge animation set with the element used
 * as the effect's sprite palette, and stays 0 while the action has no attack
 * to animate. The failed-condition path is written as its own `packed = 0`
 * block: the target keeps that block, and a pre-initialized `packed` lets GCC
 * thread the failing tests straight to the call. Items 0x4a..0x4c (the
 * elemental guns) and ability 0x189 (Ninja Ball) are hard-coded exceptions. */
void effect_store_targets_and_display_types(s32 ability, battle_unit_t* unit) {
    effect_secondary_init_t init;
    battle_unit_t* target;
    battle_unit_t* spectator;
    unit_stats_t* stats;
    s32 i;
    s32 j;
    s32 spectators;
    u32 packed;
    u8 item;
    battle_strike_work_t* block;

    init.target_count = unit->strike.target_count;
    init.targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    init.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = unit->runtime_id;
    main_util_set_svector(&init.target_tile.data.tile, unit->strike.target_new_x, unit->strike.target_new_map_level,
        unit->strike.target_new_y);
    block = &unit->strike;
    if (init.target_count != 0) {
        for (i = 0; i < init.target_count; i++) {
            target = battle_unit_get_by_battle_id(block->target_list[i]);
            init.targets[i].target_type = EFFECT_TARGET_TYPE_UNIT;
            init.targets[i].data.unit.runtime_id = target->runtime_id;
            {
                unit_stats_t* target_stats;

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
        }
    } else if (unit->strike.skip_empty_tile_effect == 0) {
        init.target_count = 1;
        init.targets[0].target_type = EFFECT_TARGET_TYPE_TILE;
        init.targets[0].data.tile.vx = unit->strike.target_new_x;
        init.targets[0].data.tile.vz = unit->strike.target_new_y;
        init.targets[0].data.tile.vy = unit->strike.target_new_map_level;
    }
    spectators = 0;
    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        if (i != unit->runtime_id) {
            spectator = battle_unit_get_by_runtime_id(i);
            if (spectator != 0) {
                stats = spectator->stats;
                if (stats != 0) {
                    for (j = 0; j < block->target_count; j++) {
                        if (stats->battle_id == block->target_list[j]) {
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
    if (block->last_attack_id == 0 || block->reaction_occurred != 0 || g_action_phase != ACTION_PHASE_ACTING_UNIT
        || (unit->used_item_or_weapon_id - ITEM_ID_BLAZE_GUN) < 2U
        || (item = unit->used_item_or_weapon_id) == ITEM_ID_BLAST_GUN) {
        packed = 0;
    } else {
        if (ability == ABILITY_ID_THROW_BALL) {
            packed = g_battle_ability_animation_data[ABILITY_ID_THROW_BALL].charge_animation_set_id;
            packed |= g_main_item_weapon_data[item].element << 16;
        } else {
            packed = g_battle_ability_animation_data[ability].charge_animation_set_id;
            packed |= g_main_ability_range_data[ability].element << 16;
        }
    }
    effect_set_ability_animation(packed, ability, &init);
}

/* Announce the next unit whose charged or performing action comes due.
 *
 * Each of the three action phases is polled in turn; a phase that produces a
 * unit id publishes the announcement state, resolves the spell quote and stops
 * the scan.  Returns 1 once an announcement was set up, 0 when all three phases
 * had nothing pending. */
s32 battle_state_announce_next_charged_action(void) {
    action_command_t command;
    battle_unit_t* unit;
    battle_unit_t* actor;
    s32 result;
    s32 ability_id;
    unit_stats_t* actor_stats;

    unit = battle_unit_get_source();
    do {
        switch (g_action_phase) {
        case ACTION_PHASE_PRE_ACTION:
            result = battle_reaction_prepare_hamedo_for_pending_action(unit->stats->battle_id, &command.ability_id);
            if (result != -1) {
                g_battle_game_state = BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT;
                actor = battle_unit_get_by_battle_id(result & 0xffff);
                g_battle_casting_runtime_id = actor->runtime_id;
                command.skillset_id = 0;
                actor_stats = actor->stats;
                text_select_ability_announcement(&command, actor_stats->battle_id, 1);
                ability_id = command.ability_id;
                result = 0;
                actor->continue_attack_count = 0;
                g_action_post_action = 0;
                actor->sp2_ability_id = ability_id;
                actor->used_ability_id = ability_id;
            }
            break;
        case ACTION_PHASE_ACTING_UNIT:
            result = action_init_current_data(unit->stats->battle_id);
            if (result == 0) {
                g_battle_game_state = BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT;
                g_battle_casting_runtime_id = unit->runtime_id;
                text_select_ability_announcement(&unit->stats->action_command, unit->stats->battle_id, 1);
                ability_id = unit->command_state.ai.data.action.ability_id;
                unit->continue_attack_count = 0;
                g_action_post_action = 0;
                unit->sp2_ability_id = ability_id;
                unit->used_ability_id = ability_id;
            } else {
                if (result >= 0) {
                    if (result < 4) {
                        battle_gfx_activate_numerical_sprite_data(unit, result);
                    }
                }
                battle_unit_set_animation_based_on_status(unit);
                battle_unit_update_post_command_animation_display(unit);
                result = -1;
            }
            break;
        case ACTION_PHASE_REACTING_UNIT:
            result = battle_reaction_prepare_next(&command.ability_id);
            if (result != -1) {
                g_battle_game_state = BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT;
                actor = battle_unit_get_by_battle_id(result & 0xffff);
                g_battle_casting_runtime_id = actor->runtime_id;
                command.skillset_id = 0;
                actor_stats = actor->stats;
                text_select_ability_announcement(&command, actor_stats->battle_id, 1);
                ability_id = command.ability_id;
                result = 0;
                actor->continue_attack_count = 0;
                g_action_post_action = 0;
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

/* Finalizes the casting unit's action and picks its attack animation.
 *
 * Each target's attack result is resolved first; for Fall (ability 0x200) in
 * the later phase the targets are moved to the knockback destination instead.
 * An ability picks the ability animation unless a reaction occurred, while a
 * plain attack or an elemental gun picks the weapon strike.
 *
 * continue_attack_count is nonzero on the follow-up strikes of a continued
 * attack. */
void action_set_target_coords_and_attacker_anim(void) {
    battle_unit_t* unit;
    battle_unit_t* target;
    battle_strike_work_t* action;
    s32 i;

    g_main_system_animation_speed = 1;
    g_battle_game_state = BATTLE_GAME_STATE_SECONDARY_EFFECT;
    unit = battle_unit_get_casting();
    unit->state_frame_counter = 0;
    if (unit->continue_attack_count == 0) {
        battle_unit_face_towards_action_target(unit, 0);
    }
    action = &unit->strike;
    if (action->target_count != 0) {
        for (i = 0; i < action->target_count; i++) {
            target = battle_unit_get_by_battle_id(action->target_list[i]);
            if (target != 0) {
                target->pending_attack_result = action_finalize_attack_and_flag_reactions(target->stats->battle_id);
                if (target->pending_attack_result == -1) {
                    battle_unit_find_relocation_tile(target->stats->battle_id, &target->dismount);
                }
            }
        }
    }

    if (g_action_phase == ACTION_PHASE_ACTING_UNIT) {
        if (unit->used_ability_id != 0 && !IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)
            && action->reaction_occurred == 0) {
            battle_unit_select_attack_animation_for_ability(unit, battle_unit_get_by_battle_id(action->target_list[0]));
        } else if (unit->used_ability_id == 0 || IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)) {
            if (action->target_count != 0) {
                battle_unit_select_weapon_attack_animation(unit, battle_unit_get_by_battle_id(action->target_list[0]));
            } else {
                battle_unit_select_weapon_attack_animation(unit, 0);
            }
        }
    } else if (unit->used_ability_id == ABILITY_ID_KNOCKBACK) {
        for (i = 0; i < unit->strike.target_count; i++) {
            target = battle_unit_get_by_battle_id(action->target_list[i]);
            if (target != 0) {
                target->map_x = unit->strike.target_new_x;
                target->map_y = unit->strike.target_new_y;
                target->map_z = unit->strike.target_new_map_level;
                battle_unit_set_tile_position(
                    target->stats->battle_id, target->map_x, target->map_y, target->map_z, target->facing / 1024);
                battle_unit_set_move_and_world_coords(target);
            }
        }
    } else if (action->reaction_ability_id != ABILITY_ID_REACTION_REFLECT) {
        if (unit->used_ability_id != 0 && !IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)
            && unit->continue_attack_count == 0) {
            if (action->reaction_occurred == 0) {
                battle_unit_select_attack_animation_for_ability(
                    unit, battle_unit_get_by_battle_id(action->target_list[0]));
            }
        } else if (unit->used_ability_id == 0 || IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)) {
            if (action->target_count != 0) {
                battle_unit_select_weapon_attack_animation(unit, battle_unit_get_by_battle_id(action->target_list[0]));
            } else {
                battle_unit_select_weapon_attack_animation(unit, 0);
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
void action_set_damage_display_type_based_on_ability(void) {
    battle_unit_t* unit;
    u16 raw;
    s32 ability;
    s32 target;

    g_main_system_animation_speed = 1;
    unit = battle_unit_get_casting();
    action_resolve_ability_strike(unit->stats->battle_id, &unit->strike);
    raw = unit->strike.last_attack_id;
    /* The target reads 0x1a6 as a signed 16-bit value. */
    target = unit->strike.reaction_id;
    ability = raw & 0xffff;
    unit->used_ability_id = raw;
    battle_gfx_set_thrown_item_graphic_palette(unit->strike.used_weapon_id, unit);
    if (((target != 0) && (ability != target)) || (ability == ABILITY_ID_KNOCKBACK) || (ability == 0)
        || (effect_load_ability(ability) != 0)) {
        action_set_target_coords_and_attacker_anim();
        return;
    }
    g_battle_game_state = BATTLE_GAME_STATE_START_EFFECT_FILE_OPEN;
    effect_store_targets_and_display_types(ability, unit);
}

void battle_state_enter_open_sp2_files(void) {
    g_main_system_animation_speed = 1;
    g_battle_game_state = BATTLE_GAME_STATE_OPEN_SP2_FILES;
    g_battle_gfx_sp2_data = 0;
}

/* Enter the pre-attack animation state: clear the tile marks and start the used
 * ability's charge animation, or the standing animation when the ability has no
 * charge animation set. */
void battle_state_enter_pre_attack_animation(void) {
    battle_unit_t* unit;
    u16 ability;

    target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 2);
    g_battle_game_state = BATTLE_GAME_STATE_PRE_ATTACK_ANIMATION;
    unit = battle_unit_get_source();
    ability = unit->used_ability_id;
    unit->state_frame_counter = 0;
    if (g_battle_ability_animation_data[ability].charge_animation_set_id != 0) {
        battle_unit_start_ability_charge_animation(unit);
    } else {
        /* facing is read signed (lh) here. */
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_STAND, unit->facing, unit);
    }
    action_clear_at_list_active();
}

/* Enter the action-cast state for the source unit, unless the scenario check
 * starts an event first; the camera turns when the unit's tile is hidden. */
void battle_state_enter_action_cast(void) {
    battle_unit_t* unit;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ACTION_CAST;
    g_action_post_action = 0;
    unit = battle_unit_get_source();
    if (battle_state_try_enter_scenario_event(unit) == 0) {
        g_battle_game_state = BATTLE_GAME_STATE_ACTION_CAST;
        camera_rotate_when_unit_tile_not_visible(unit);
    }
}

/* Starts the acting unit's confirmed ability: marks the action taken, faces the
 * cursor and runs the attack preview. A preview result of 1 or 3 enters the
 * commence-attack phase; 0 starts the charge animation. */
void action_execute_ability(void) {
    battle_unit_t* unit;
    s32 result;

    menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0, 0);
    unit = battle_unit_get_source();
    action_set_only_action_taken(unit->stats->battle_id);
    unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
    battle_unit_face_toward_cursor(unit);
    result = action_call_attack_preparation_at_preview(&unit->command_state.ai.data.action);
    unit->attack_phase_state = result;
    switch (result) {
    case BATTLE_ATTACK_PREPARATION_CHARGE:
        battle_unit_start_ability_charge_hold_animation(unit);
        break;
    case BATTLE_ATTACK_PREPARATION_IMMEDIATE:
    case BATTLE_ATTACK_PREPARATION_JUMP:
        camera_disable_map_pan();
        g_battle_game_state = BATTLE_GAME_STATE_COMMENCE_ATTACK_PHASE;
        g_action_post_action = 0;
        unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
        if (g_battle_ability_animation_data[unit->used_ability_id].charge_animation_set_id != 0) {
            battle_unit_start_ability_charge_hold_animation(unit);
        }
        return;
    case BATTLE_ATTACK_PREPARATION_CHANGE_EQUIPMENT:
    default:
        main_system_handle_pointer_exception(0x13);
        break;
    }
    battle_unit_update_post_command_animation_display(unit);
}

void battle_state_enter_commence_attack_phase(void) {
    unit_stats_t* stats;
    battle_unit_t* unit;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_COMMENCE_ATTACK_PHASE;
    unit = battle_unit_get_source();
    stats = unit->stats;
    unit->command_state.ai.data.action = stats->action_command;
    /* action_command.ability_id is s16 in the header; the target loads it with lhu. */
    unit->used_ability_id = unit->stats->action_command.ability_id;
    unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
    if (unit->used_ability_id != 0) {
        battle_unit_start_ability_charge_hold_animation(unit);
    }
}

/* Enter the ability-preview-help state and start the help menu. Nothing in the
 * game calls it. */
void battle_menu_start_help_thread(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HELP;
    g_battle_menu_help_opening = 1;
    menu_start_building_thread();
}

/*
 * Seed the source unit's pending action target and run target-tile selection.
 *
 * During ability-preview phase 2 the target is the unit's own tile; for
 * player-controlled units it is the current cursor tile.
 * target_calculate_for_menu_types's result
 * picks the next step: 0/1 enter the ability-preview state, 2 restores the
 * unit and stops game flow, and -1 returns to the idle action menu.
 */
void target_select_tile(void) {
    battle_unit_t* unit;
    s32 result;
    s32 prev;

    unit = battle_unit_get_source();
    unit->state_frame_counter = 0;
    if (unit->ability_preview_phase == TARGET_PANELS_NO_TARGETING) {
        main_util_set_svector(
            &unit->command_state.ai.data.action.target_position, unit->map_x, unit->map_z, unit->map_y);
        unit->command_state.ai.data.action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
    } else if (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
        main_util_set_svector(&unit->command_state.ai.data.action.target_position, g_battle_cursor_x, g_battle_cursor_z,
            g_battle_cursor_y);
        unit->command_state.ai.data.action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
    }
    result = target_calculate_for_menu_types(&unit->command_state.ai.data.action);
    unit->target_select_result = result;
    switch (result) {
    case 0:
    case 1:
        camera_enable_map_pan();
        prev = g_battle_cursor_input_mode;
        g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING;
        g_battle_cursor_input_mode = BATTLE_CURSOR_INPUT_FREE_CURSOR;
        g_battle_cursor_input_mode_saved = prev;
        target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_TARGETS, 3);
        target_store_cursor_casting_unit_name_and_data();
        if (g_main_game_options.fields.target_flashing == GAME_OPTION_ON) {
            battle_gfx_tint_all_units_by_team();
        }
        break;
    case 2:
        camera_disable_map_pan();
        battle_state_enter_pre_attack_animation();
        target_store_cursor_casting_unit_name_and_data();
        break;
    case -1:
        battle_menu_dispatch_idle_action_menu();
        break;
    }
    action_clear_at_list_active();
}

/* Enter the illegal-range state, reached when the chosen tile lacks the
 * ability-range mark: opens action-confirm option 5 for the source unit. */
void battle_state_enter_target_out_of_range(void) {
    battle_unit_t* unit;
    unit_stats_t* stats;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ILLEGAL_RANGE;
    unit = battle_unit_get_source();
    stats = unit->stats;
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 5, stats->battle_id, 0,
        unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Start target selection for the source unit's previewed action and return its
 * ability_preview_phase (target_panels_e).
 *
 * An action with a target to choose enters the targeting-range state with the
 * range tiles shown and the cursor unit as preview target. An action without
 * targeting selects its tile at once. With no valid panels the idle action menu
 * reopens. */
s32 target_set_boxes_red(void) {
    battle_unit_t* unit;

    unit = battle_unit_get_source();
    unit->state_frame_counter = 0;
    switch (unit->ability_preview_phase) {
    case TARGET_PANELS_UNIT_OR_TILE:
    case TARGET_PANELS_TILE_ONLY:
        camera_disable_map_pan();
        g_battle_game_state = BATTLE_GAME_STATE_TARGETING_RANGE;
        target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_RANGE, 2);
        target_store_cursor_unit_as_preview_target();
        if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
            action_set_at_list_active();
        } else {
            action_clear_at_list_active();
        }
        break;
    case TARGET_PANELS_NO_TARGETING:
        camera_disable_map_pan();
        target_select_tile();
        target_store_cursor_unit_as_preview_target();
        break;
    case TARGET_PANELS_INVALID:
    case TARGET_PANELS_NONE:
        battle_menu_dispatch_idle_action_menu();
        break;
    }
    return unit->ability_preview_phase;
}

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

void move_return_to_previous_map_square(battle_unit_t* unit) {
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
    battle_unit_store_animation_facing_movement_data(unit->animation.sequence_id >> 1, unit->facing, unit);
}

/* Cancel a move at the confirm prompt. The unit returns to its previous tile;
 * a mount takes its rider back with it, a unit that was riding is put back on
 * its mount, and a mount it climbed onto during the move is freed. */
void move_undo_unit_move(battle_unit_t* unit) {
    battle_unit_t* other;

    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        other->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
        battle_unit_update_display_by_runtime_id(other->runtime_id);
    }
    move_return_to_previous_map_square(unit);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
        other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (other != 0) {
            move_return_to_previous_map_square(other);
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
 * After `rider` moves, re-registers its map position and, when it is the
 * rider of a mount (mount state MOUNT), copies tile, destination, real and
 * screen position to the mount 10 units / 0xa000 lower, then finishes the
 * acting unit's move. The two position blocks are copied through local block
 * types because the target uses struct copies (four lw/sw, and lwl/lwr for
 * the 16-bit-aligned screen block) that per-field stores do not reproduce.
 */

void battle_unit_copy_rider_data_to_mount(battle_unit_t* unit, battle_unit_t* rider) {
    battle_unit_t* mount;

    if (rider->stats != 0) {
        battle_unit_set_tile_position(
            rider->stats->battle_id, rider->map_x, rider->map_y, rider->map_z, (rider->facing / 1024));
        if (rider->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
            mount = battle_unit_get_by_runtime_id(rider->mount_partner_runtime_id);
            if (mount != 0) {
                mount->map_x = mount->movement.destination_x = rider->map_x;
                mount->map_y = mount->movement.destination_y = rider->map_y;
                mount->map_z = mount->movement.destination_z = rider->map_z;
                mount->real = rider->real;
                mount->world_coords = rider->world_coords;
                mount->world_coords.vy -= 10;
                mount->real.vy -= 0xa000;
                battle_unit_set_tile_position(
                    mount->stats->battle_id, rider->map_x, rider->map_y, rider->map_z, (rider->facing / 1024));
            }
        }
    }
    unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_MOVED;
    action_set_only_movement_taken(unit->stats->battle_id);
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)
        && !(unit->path.flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT)) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
    }
    battle_gfx_update_unit_palettes();
    move_init_post_movement_display();
}

/* MoveConfirmMenu game-state handler. Command 7 keeps the move and finishes it.
 * Command 8 or 0xff undoes it and returns to choosing a destination, except after
 * a teleport, which is kept like command 7. */
void battle_state_handle_move_confirm_menu_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    battle_unit_t* source_unit;
    battle_unit_t* casting_unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    source_unit = battle_unit_get_source();
    casting_unit = battle_unit_get_casting();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case 8:
    case 0xff:
        if (casting_unit->path.flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT) {
        case 7:
            battle_unit_copy_rider_data_to_mount(source_unit, casting_unit);
            return;
        }
        move_undo_unit_move(casting_unit);
        battle_gfx_update_unit_palettes();
        battle_menu_close_move_help();
        return;
    }
}

/* WaitMenu game-state handler. Command 7 goes on to choosing a facing. Command 8
 * or 0xff returns to the after-command state, unless the unit's turn is over
 * (action_check_change_of_turn returns 1): then it chooses a facing too. */
void battle_state_handle_wait_menu_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    battle_unit_t* source_unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    source_unit = battle_unit_get_source();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case 8:
    case 0xff:
        if (action_check_change_of_turn(source_unit->stats->battle_id) != 1) {
            battle_state_enter_after_command();
            return;
        }
    case 7:
        action_choose_facing_for_wait();
        return;
    }
}
