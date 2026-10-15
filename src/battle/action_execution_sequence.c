/* Wait facing, strike execution, post-effect messages and post-action reports. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/main.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Enter the wait-direction state for the source unit, starting from its current
 * facing.
 *
 * A rider first takes its mount's facing. A unit the player does not control gets
 * its facing from ai_choose_wait_facing as the cursor's facing hint. */
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
