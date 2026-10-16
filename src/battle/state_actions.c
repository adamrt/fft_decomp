/* Charged action announcements, pre-attack animation, casting, target selection and display, animation speed halving,
 * and the state handlers from the free cursor through unit moving. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_ai_command.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_effect_script.h"
#include "fft/battle_gfx.h"
#include "fft/battle_gfx_resource.h"
#include "fft/battle_map_palette.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/battle_unit_animation.h"
#include "fft/battle_unit_encounter.h"
#include "fft/battle_unit_path.h"
#include "fft/data_ability.h"
#include "fft/data_item.h"
#include "fft/data_skillset.h"
#include "fft/main.h"
#include "fft/main_gfx.h"
#include "fft/main_item.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/menu_command.h"
#include "fft/menu_help.h"
#include "fft/menu_input.h"
#include "fft/menu_option.h"
#include "fft/menu_overlay.h"
#include "fft/menu_text.h"
#include "fft/menu_unit_view.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

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
static void battle_effect_store_targets_and_display_types(s32 ability, battle_unit_t* unit) {
    effect_secondary_init_t init;
    battle_unit_t* target;
    battle_unit_t* spectator;
    unit_stats_t* target_stats;
    unit_stats_t* spectator_stats;
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
        init.targets[0].data.tile.vx = unit->strike.target_new_x;
        init.targets[0].data.tile.vz = unit->strike.target_new_y;
        init.targets[0].data.tile.vy = unit->strike.target_new_map_level;
    }
    spectators = 0;
    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        if (i != unit->runtime_id) {
            spectator = battle_unit_get_by_runtime_id(i);
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
    if (block->last_attack_id == 0 || block->reaction_occurred != 0 || g_battle_action_phase != ACTION_PHASE_ACTING_UNIT
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
    battle_effect_set_ability_animation(packed, ability, &init);
}

/* Announce the next unit whose charged or performing action comes due.
 *
 * Each of the three action phases is polled in turn; a phase that produces a
 * unit id publishes the announcement state, resolves the spell quote and stops
 * the scan.  Returns 1 once an announcement was set up, 0 when all three phases
 * had nothing pending. */
s32 battle_state_announce_next_charged_action(void) {
    action_command_t command;
    battle_unit_t* unit = battle_unit_get_source();
    battle_unit_t* actor;
    s32 result;
    s32 ability_id;
    unit_stats_t* actor_stats;

    do {
        switch (g_battle_action_phase) {
        case ACTION_PHASE_PRE_ACTION:
            result = battle_reaction_prepare_hamedo_for_pending_action(unit->stats->battle_id, &command.ability_id);
            if (result != -1) {
                g_battle_game_state = BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT;
                actor = battle_unit_get_by_battle_id(result);
                g_battle_casting_runtime_id = actor->runtime_id;
                command.skillset_id = 0;
                actor_stats = actor->stats;
                text_select_ability_announcement(&command, actor_stats->battle_id, 1);
                ability_id = command.ability_id;
                result = 0;
                actor->continue_attack_count = 0;
                g_battle_action_post_action = 0;
                actor->sp2_ability_id = ability_id;
                actor->used_ability_id = ability_id;
            }
            break;
        case ACTION_PHASE_ACTING_UNIT:
            result = battle_action_init_current_data(unit->stats->battle_id);
            if (result == 0) {
                g_battle_game_state = BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT;
                g_battle_casting_runtime_id = unit->runtime_id;
                text_select_ability_announcement(&unit->stats->action_command, unit->stats->battle_id, 1);
                ability_id = unit->command_state.ai.data.action.ability_id;
                unit->continue_attack_count = 0;
                g_battle_action_post_action = 0;
                unit->sp2_ability_id = ability_id;
                unit->used_ability_id = ability_id;
            } else {
                if (result >= 0) {
                    if (result < 4) {
                        battle_gfx_queue_charge_failure_message(unit, result);
                    }
                }
                battle_unit_set_animation_based_on_status(unit);
                battle_unit_end_command(unit);
                result = -1;
            }
            break;
        case ACTION_PHASE_REACTING_UNIT:
            result = battle_reaction_prepare_next(&command.ability_id);
            if (result != -1) {
                g_battle_game_state = BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT;
                actor = battle_unit_get_by_battle_id(result);
                g_battle_casting_runtime_id = actor->runtime_id;
                command.skillset_id = 0;
                actor_stats = actor->stats;
                text_select_ability_announcement(&command, actor_stats->battle_id, 1);
                ability_id = command.ability_id;
                result = 0;
                actor->continue_attack_count = 0;
                g_battle_action_post_action = 0;
                actor->sp2_ability_id = ability_id;
                actor->used_ability_id = ability_id;
            }
            break;
        }
        if (result != -1) {
            return 1;
        }
        g_battle_action_phase = g_battle_action_phase + 1;
    } while (g_battle_action_phase < ACTION_PHASE_COUNT);
    return 0;
}

/* Blaze Gun, Glacier Gun and Blast Gun (item ids 0x4a..0x4c) play the weapon
 * strike even when an ability is used. */
#define IS_ELEMENTAL_GUN(id) ((id == ITEM_ID_BLAZE_GUN || id == ITEM_ID_GLACIER_GUN) || id == ITEM_ID_BLAST_GUN)

/* Finalizes the casting unit's action and picks its attack animation.
 *
 * Each target's attack result is resolved first; for the knockback strike
 * (ABILITY_ID_KNOCKBACK) in a later phase the targets are moved to the
 * knockback destination instead.
 * An ability picks the ability animation unless a reaction occurred, while a
 * plain attack or an elemental gun picks the weapon strike.
 *
 * continue_attack_count is nonzero on the follow-up strikes of a continued
 * attack. */
void battle_action_set_target_coords_and_attacker_anim(void) {
    battle_unit_t* unit;
    battle_unit_t* target;
    battle_strike_work_t* action;
    s32 i;

    g_main_system_vsync_interval = 1;
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
                target->pending_attack_result = battle_action_apply_target_result(target->stats->battle_id);
                if (target->pending_attack_result == -1) {
                    battle_unit_find_relocation_tile(target->stats->battle_id, &target->dismount);
                }
            }
        }
    }

    if (g_battle_action_phase == ACTION_PHASE_ACTING_UNIT) {
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
                battle_unit_set_tile_position(target->stats->battle_id, target->map_x, target->map_y, target->map_z,
                    target->facing / BATTLE_FACING_QUARTER_TURN);
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
void battle_action_start_next_strike_display(void) {
    battle_unit_t* unit;
    u16 attack_id;
    s32 ability;
    s32 reaction;

    g_main_system_vsync_interval = 1;
    unit = battle_unit_get_casting();
    battle_action_resolve_ability_strike(unit->stats->battle_id, &unit->strike);
    attack_id = unit->strike.last_attack_id;
    /* The target reads reaction_id as a signed 16-bit value. */
    reaction = unit->strike.reaction_id;
    ability = attack_id;
    unit->used_ability_id = attack_id;
    battle_gfx_set_thrown_item_graphic_palette(unit->strike.used_weapon_id, unit);
    if (((reaction != 0) && (ability != reaction)) || (ability == ABILITY_ID_KNOCKBACK) || (ability == 0)
        || (battle_effect_ability_has_no_file(ability) != 0)) {
        battle_action_set_target_coords_and_attacker_anim();
        return;
    }
    g_battle_game_state = BATTLE_GAME_STATE_START_EFFECT_FILE_OPEN;
    battle_effect_store_targets_and_display_types(ability, unit);
}

/* Enter the OpenSp2Files game state at full animation speed, with no SP2 buffer yet, so its
 * handler starts a fresh load. */
void battle_state_enter_open_sp2_files(void) {
    g_main_system_vsync_interval = 1;
    g_battle_game_state = BATTLE_GAME_STATE_OPEN_SP2_FILES;
    g_battle_gfx_sp2_data = 0;
}

/* Enter the pre-attack animation state: clear the tile marks and start the used
 * ability's charge animation, or the standing animation when the ability has no
 * charge animation set. */
void battle_state_enter_pre_attack_animation(void) {
    battle_unit_t* unit;
    u16 ability;

    battle_target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 2);
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
    battle_target_hide_cursor();
}

/* Enter the action-cast state for the source unit, unless the scenario check
 * starts an event first; the camera turns when the unit's tile is hidden. */
void battle_state_enter_action_cast(void) {
    battle_unit_t* unit;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ACTION_CAST;
    g_battle_action_post_action = 0;
    unit = battle_unit_get_source();
    if (battle_state_try_enter_scenario_event(unit) == 0) {
        g_battle_game_state = BATTLE_GAME_STATE_ACTION_CAST;
        battle_camera_rotate_when_unit_tile_not_visible(unit);
    }
}

/* Starts the acting unit's confirmed ability: marks the action taken, faces the
 * cursor and commits the attack preparation. An immediate or Jump action enters
 * the commence-attack phase, with the charge-hold animation when the ability has a
 * charge set. A charged action starts its charge-hold animation and ends the
 * command until the charge comes due; any other result raises pointer exception
 * 19 and ends the command. */
void battle_action_execute_ability(void) {
    battle_unit_t* unit;
    s32 result;

    menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0, 0);
    unit = battle_unit_get_source();
    battle_action_set_only_action_taken(unit->stats->battle_id);
    unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
    battle_unit_face_toward_cursor(unit);
    result = battle_action_commit_attack_preparation(&unit->command_state.ai.data.action);
    unit->attack_phase_state = result;
    switch (result) {
    case BATTLE_ATTACK_PREPARATION_CHARGE:
        battle_unit_start_ability_charge_hold_animation(unit);
        break;
    case BATTLE_ATTACK_PREPARATION_IMMEDIATE:
    case BATTLE_ATTACK_PREPARATION_JUMP:
        battle_camera_disable_map_pan();
        g_battle_game_state = BATTLE_GAME_STATE_COMMENCE_ATTACK_PHASE;
        g_battle_action_post_action = 0;
        unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
        if (g_battle_ability_animation_data[unit->used_ability_id].charge_animation_set_id != 0) {
            battle_unit_start_ability_charge_hold_animation(unit);
        }
        return;
    case BATTLE_ATTACK_PREPARATION_CHANGE_EQUIPMENT:
    default:
        main_system_handle_pointer_exception(19);
        break;
    }
    battle_unit_end_command(unit);
}

/* Enter the commence-attack phase for the source unit: copy the action command stored in its
 * battle stats into its runtime action, mark it acted and start the charge-hold animation when it
 * uses an ability. */
void battle_state_enter_commence_attack_phase(void) {
    unit_stats_t* stats;
    battle_unit_t* unit;

    battle_camera_disable_map_pan();
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
    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HELP;
    g_battle_menu_help_opening = 1;
    menu_start_help_menu_thread();
}

/*
 * Seed the source unit's pending action target and run target-tile selection.
 *
 * An action without targeting (TARGET_PANELS_NO_TARGETING) targets the unit's own
 * tile; otherwise a player-controlled unit targets the cursor tile and an AI unit
 * keeps the target it chose. battle_target_calculate_for_menu_types's result picks
 * the next step: 0 or 1 (affected tiles marked or not) enter the ability-preview
 * state with the free cursor, 2 (a menu type without targeting) goes straight to
 * the pre-attack animation, and -1 (an invalid action) returns to the idle action
 * menu.
 */
void battle_target_select_tile(void) {
    battle_unit_t* unit = battle_unit_get_source();
    s32 result;
    s32 saved_input_mode;

    unit->state_frame_counter = 0;
    if (unit->ability_preview_phase == TARGET_PANELS_NO_TARGETING) {
        main_util_set_svector(
            &unit->command_state.ai.data.action.target_position, unit->map_x, unit->map_z, unit->map_y);
        unit->command_state.ai.data.action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
    } else if (unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) {
        main_util_set_svector(&unit->command_state.ai.data.action.target_position, g_battle_cursor_x, g_battle_cursor_z,
            g_battle_cursor_y);
        unit->command_state.ai.data.action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
    }
    result = battle_target_calculate_for_menu_types(&unit->command_state.ai.data.action);
    unit->target_select_result = result;
    switch (result) {
    case 0:
    case 1:
        battle_camera_enable_map_pan();
        saved_input_mode = g_battle_cursor_input_mode;
        g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING;
        g_battle_cursor_input_mode = BATTLE_CURSOR_INPUT_FREE_CURSOR;
        g_battle_cursor_input_mode_saved = saved_input_mode;
        battle_target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_TARGETS, 3);
        battle_target_store_cursor_casting_unit_name_and_data();
        if (g_main_game_options.fields.target_flashing == GAME_OPTION_ON) {
            battle_gfx_tint_all_units_by_team();
        }
        break;
    case 2:
        battle_camera_disable_map_pan();
        battle_state_enter_pre_attack_animation();
        battle_target_store_cursor_casting_unit_name_and_data();
        break;
    case -1:
        battle_menu_dispatch_idle_action_menu();
        break;
    }
    battle_target_hide_cursor();
}

/* Enter the illegal-range state, reached when the chosen tile lacks the
 * ability-range mark: opens action-confirm option 5 for the source unit. */
void battle_state_enter_target_out_of_range(void) {
    battle_unit_t* unit;
    unit_stats_t* stats;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ILLEGAL_RANGE;
    unit = battle_unit_get_source();
    stats = unit->stats;
    menu_init_system_function(MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 5, stats->battle_id, 0,
        unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Start target selection for the source unit's previewed action and return its
 * ability_preview_phase (target_panels_e).
 *
 * An action with a target to choose enters the targeting-range state with the
 * range tiles shown and the cursor unit as preview target. An action without
 * targeting selects its tile at once. With no valid panels the idle action menu
 * reopens. */
s32 battle_target_begin_tile_selection(void) {
    battle_unit_t* unit = battle_unit_get_source();

    unit->state_frame_counter = 0;
    switch (unit->ability_preview_phase) {
    case TARGET_PANELS_UNIT_OR_TILE:
    case TARGET_PANELS_TILE_ONLY:
        battle_camera_disable_map_pan();
        g_battle_game_state = BATTLE_GAME_STATE_TARGETING_RANGE;
        battle_target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_RANGE, 2);
        battle_target_store_cursor_unit_as_preview_target();
        if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
            battle_target_show_cursor();
        } else {
            battle_target_hide_cursor();
        }
        break;
    case TARGET_PANELS_NO_TARGETING:
        battle_camera_disable_map_pan();
        battle_target_select_tile();
        battle_target_store_cursor_unit_as_preview_target();
        break;
    case TARGET_PANELS_INVALID:
    case TARGET_PANELS_NONE:
        battle_menu_dispatch_idle_action_menu();
        break;
    }
    return unit->ability_preview_phase;
}

/* Enter the ConfirmAction state and open the action-confirm prompt for the source unit. The prompt
 * option depends on how the target is chosen and on whether a unit stands on the cursor tile: 1
 * for a unit an ability can follow, 4 for a unit under a tile-only ability, 3 for a unit with a
 * self-centered action, 2 for an empty tile and 0 otherwise. */
void battle_action_confirm(void) {
    battle_unit_t* unit;
    battle_unit_t* target;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_CONFIRM_ACTION;
    unit = battle_unit_get_source();
    target = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    switch (unit->ability_preview_phase) {
    case TARGET_PANELS_UNIT_OR_TILE:
        if (target != 0) {
            menu_init_system_function(MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 1, unit->stats->battle_id, 0,
                unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            menu_init_system_function(MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 2, unit->stats->battle_id, 0,
                unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        }
        break;
    case TARGET_PANELS_TILE_ONLY:
        if (target != 0) {
            menu_init_system_function(MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 4, unit->stats->battle_id, 0,
                unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            menu_init_system_function(MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 2, unit->stats->battle_id, 0,
                unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        }
        break;
    case TARGET_PANELS_NO_TARGETING:
        if (target != 0) {
            menu_init_system_function(MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 3, unit->stats->battle_id, 0,
                unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            menu_init_system_function(MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 2, unit->stats->battle_id, 0,
                unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        }
        break;
    default:
        menu_init_system_function(MENU_SYSTEM_COMMAND_ACTION_CONFIRM, 0, unit->stats->battle_id, 0,
            unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED);
        break;
    }
}

/* Starts the repeating tint that marks the candidates of an auto-battle target
 * choice: red on every unit of another team than the casting unit's for
 * AI_COMMAND_AUTOBATTLE_TARGETED_SELECT, blue on every unit of its own
 * team for AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT. Other modes tint
 * nothing. */
static void battle_gfx_highlight_all_units_blue_or_red(s32 mode) {
    battle_unit_t* caster = battle_unit_get_casting();
    battle_unit_t* unit;
    s32 i;

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
            if ((unit->stats->team_flags & UNIT_TEAM_MASK) != (caster->stats->team_flags & UNIT_TEAM_MASK)) {
                battle_map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, 4, MAP_CLUT_ROW_UNITS, i, 0, 0x10, 0, 0);
            }
            continue;
        case AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT:
            if ((unit->stats->team_flags & UNIT_TEAM_MASK) == (caster->stats->team_flags & UNIT_TEAM_MASK)) {
                battle_map_modify_palette(MAP_COLOR_MODE_BASE_REPEAT, 4, MAP_CLUT_ROW_UNITS, i, 0, 0, 0, 0x18);
            }
            continue;
        }
    }
}

/* Enter the target-select-start state: open the targeting prompt for the casting
 * unit. */
static void battle_state_enter_target_select_start(void) {
    battle_unit_t* unit;
    unit_stats_t* stats;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT_START;
    unit = battle_unit_get_casting();
    stats = unit->stats;
    menu_init_system_function(MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 0, stats->battle_id, 0, 1);
    battle_target_show_cursor();
}

/* Enter the target-select state: show the panel for the cursor tile and tint the
 * units the casting unit's selection command may pick, the other teams' red for a
 * targeted selection and its own team's blue for a recovery selection. */
void battle_state_enter_target_select(void) {
    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT;
    battle_target_store_cursor_unit_name_and_data();
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
    battle_unit_t* occupant;
    battle_unit_t* caster;
    unit_stats_t* stats;
    s32 command;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT_DENIED;
    occupant = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (occupant != 0) {
        stats = occupant->stats;
        if (stats != 0) {
            menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, stats->battle_id, 0);
        }
    }
    caster = battle_unit_get_casting();
    if (occupant == 0) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 3, caster->stats->battle_id, 0, 1);
        return;
    }
    command = caster->target_select_command;
    if (command == AI_COMMAND_AUTOBATTLE_TARGETED_SELECT) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 1, caster->stats->battle_id, 0, 1);
        return;
    }
    if (command == AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 2, caster->stats->battle_id, 0, 1);
    }
}

/* Enter the target-select-confirm state: show the summary of the unit under the
 * cursor and open the target-confirm prompt (option 0) for the casting unit. */
void battle_state_enter_target_select_confirm(void) {
    battle_unit_t* unit;
    unit_stats_t* stats;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT_CONFIRM;
    unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (unit != 0) {
        stats = unit->stats;
        if (stats != 0) {
            menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, stats->battle_id, 0);
        }
    }
    stats = battle_unit_get_casting()->stats;
    menu_init_system_function(MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 0, stats->battle_id, 0, 1);
}

/* Enter the target-display-start state: show the tile info panel and open the
 * targeting prompt for the casting unit. */
static void battle_state_enter_target_display_start(void) {
    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_DISPLAY_START;
    menu_start_unit_view_thread(UNIT_VIEW_MODE_TILE_INFO, 0xFF, 0xFF);
    menu_init_system_function(
        MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 0, battle_unit_get_casting()->stats->battle_id, 0, 1);
    g_battle_action_post_action = 0;
    battle_target_show_cursor();
}

/* Enter the target-display state: move the cursor to the casting unit's
 * auto-battle target, turn the camera when its tile is hidden and show its
 * summary panel. */
void battle_state_enter_target_display(void) {
    unit_stats_t* caster_stats;
    battle_unit_t* target;
    unit_stats_t* target_stats;

    battle_camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_DISPLAY;
    caster_stats = battle_unit_get_casting()->stats;
    target = battle_unit_get_by_battle_id(caster_stats->auto_battle_target);
    battle_target_move_cursor_to_unit(target);
    battle_camera_rotate_when_unit_tile_not_visible(target);
    if (target != 0) {
        target_stats = target->stats;
        if (target_stats != 0) {
            menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, target_stats->battle_id, 0);
        }
    }
}

/* Enter the Effect game state at two vertical blanks per frame. No code calls it
 * or battle_state_enter_effect_playback, the only other entry to that state. */
void battle_state_enter_effect_state(void) {
    g_main_system_vsync_interval = 2;
    g_battle_game_state = BATTLE_GAME_STATE_EFFECT;
    battle_target_show_cursor();
}

/* Enter the MapJumpingOut game state at two vertical blanks per frame. No code
 * calls it. */
void battle_state_enter_map_jumping_out(void) {
    g_main_system_vsync_interval = 2;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_OUT;
}

/* Enter the MapJumpingIn game state at two vertical blanks per frame. No code
 * calls it. */
void battle_state_enter_map_jumping_in(void) {
    g_main_system_vsync_interval = 2;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_IN;
}

/* Queue the CloseBattle game state; callers always pass transition step 8. */
void battle_state_halve_animation_speed_and_queue_close(s32 transition_step, s32 close_flow_state) {
    g_main_system_vsync_interval = 2;
    g_battle_game_state = BATTLE_GAME_STATE_CLOSE_BATTLE;
    g_battle_state_screen_fade_step = transition_step;
    g_battle_game_flow_state = close_flow_state;
}

/* Project the cursor tile to the screen; while g_battle_action_at_list_active is set, also draw
 * the map cursor and its tile glow. */
void battle_target_update_cursor(void) {
    SVECTOR position;
    SVECTOR raised;
    VECTOR projected;

    if (g_battle_action_at_list_active != 0) {
        battle_target_calculate_tile_coords_with_cursor_glow();
        return;
    }
    battle_target_project_cursor_tile_to_screen(&projected, &position, &raised);
}

/* Per-frame unit update shared by most game-state handlers; it reads no input.
 *
 * Projects the units, animates them unless the status screen owns the frame,
 * updates the status bubbles and unit graphics, and projects the cursor tile,
 * drawing the map cursor and tile glow while g_battle_action_at_list_active is
 * set. */
void battle_state_update_units(void) {
    battle_gfx_project_all_units();
    if (g_battle_menu_status_screen_selected != 1) {
        battle_unit_update_and_animate_units();
    }
    battle_gfx_update_status_bubbles_and_graphics();
    battle_target_update_cursor();
}

/* Free-cursor game state: camera and cursor input, then the button actions
 * (help, team highlight, act menu, move range, AT list and mini menu). */
void battle_state_handle_free_cursor_state(void) {
    battle_unit_t* unit;
    s32 input;
    RECT rect; /* never used; retail reserves its 8 bytes */

    battle_camera_handle_rotation_input();
    battle_camera_update_zoom_input();
    battle_camera_update_tilt_input();
    battle_target_move_cursor_by_input();
    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    input = g_main_input_pressed_buttons;
    if (input & PSX_PAD_SELECT) {
        battle_action_clear_at_list_id();
        battle_menu_open_free_cursor_help();
    } else if (input & PSX_PAD_SQUARE) {
        battle_action_clear_at_list_id();
        battle_state_enter_highlight_units_by_team();
    } else if (input & PSX_PAD_CIRCLE) {
        battle_target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 0);
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        if (battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z) != 0) {
            battle_menu_dispatch_idle_action_menu();
        } else {
            unit = battle_unit_get_source();
            if (unit != 0) {
                battle_target_move_cursor_to_unit(unit);
                battle_menu_set_next_script_action_menus();
            }
        }
    } else if (input & PSX_PAD_CROSS) {
        battle_action_clear_at_list_id();
        unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        if (unit != 0) {
            g_battle_casting_runtime_id = unit->runtime_id;
            battle_target_show_move_range_in_free_cursor();
        }
    } else if (input & PSX_PAD_START) {
        if (g_main_game_options.fields.multi_height_cursor_speed != GAME_MULTI_HEIGHT_CURSOR_SPEED_STOP) {
            main_sound_play_sfx(MAIN_SFX_CONFIRM);
            battle_action_show_next_at_list_entry();
            battle_target_store_cursor_unit_name_and_data();
        }
    } else if (input & PSX_PAD_TRIANGLE) {
        battle_action_clear_at_list_id();
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        battle_menu_open_free_cursor_mini_menu();
    }
}

/* FreeCursorHelp game-state handler: when the help window has gone, return to the
 * free cursor. */
void battle_state_handle_free_cursor_help_state(void) {
    if (menu_get_thread_activity() != 2) {
        g_battle_menu_help_opening = 0;
        battle_state_set_free_cursor();
    }
    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* HighlightUnits game-state handler: releasing square restores the cursor input
 * mode and the unit palettes and returns to the free cursor. */
void battle_state_handle_highlight_units_state(void) {
    if (g_main_input_released_buttons & PSX_PAD_SQUARE) {
        g_battle_cursor_input_mode = g_battle_cursor_input_mode_saved;
        battle_state_set_free_cursor();
        battle_gfx_restore_unit_palettes();
    }
    battle_camera_handle_rotation_input();
    battle_camera_update_zoom_input();
    battle_camera_update_tilt_input();
    battle_state_update_units();
}

/* OpenActionMenus game-state handler: once the window has closed (command 7, 8 or
 * 0xff) and the camera has settled, open the idle action menu of the unit under
 * the cursor. */
void battle_state_handle_open_action_menus_state(void) {
    s32 selected_command;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        g_battle_action_post_action = 1;
    }
    if ((g_battle_camera_step_vector.vx | g_battle_camera_step_vector.vy | g_battle_camera_step_vector.vz) == 0
        && g_battle_camera_rotation_action == CAMERA_ROTATION_NONE && g_battle_action_post_action != 0) {
        battle_menu_dispatch_idle_action_menu();
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
 * The kind is switched as s32: the unsigned enum type makes GCC emit sltiu
 * where the target has slti. The empty cases for the unresolved commands 0xfe
 * and 0xff keep the switch a compare tree with the target's split: a
 * two-value range above 0x19 disables the jump table (range > 10 * count). */
void battle_state_handle_idling_action_menus_state(void) {
    ai_command_t* command;
    battle_unit_t* unit;
    s32 result;

    if (menu_get_thread_activity() == 0) {
        battle_state_set_free_cursor();
    }
    if (g_main_input_pressed_buttons & PSX_PAD_SELECT) {
        battle_menu_open_help();
    }
    battle_state_update_units();
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
        battle_action_clear_at_list_id();
        battle_action_handle_move_command();
        break;
    case AI_COMMAND_MOVE_SELF:
        g_battle_casting_runtime_id = g_battle_unit_source_runtime_id;
        battle_action_clear_at_list_id();
        battle_action_handle_move_command();
        break;
    case AI_COMMAND_ACT:
        battle_action_clear_at_list_id();
        switch (command->data.action.skillset_id) {
        case SKILLSET_ID_DEFEND:
            unit->command_state.ai = *command;
            battle_status_inflict_defending_to_battle_id(unit->stats->battle_id);
            unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            battle_action_set_only_action_taken(unit->stats->battle_id);
            battle_state_enter_after_command();
            break;
        case SKILLSET_ID_EQUIP_CHANGE:
            unit->command_state.ai = *command;
            unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
            battle_status_enable_disable_acting(unit->stats);
            battle_ai_init_unit_abilities(unit->stats->battle_id);
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            battle_action_set_only_action_taken(unit->stats->battle_id);
        case SKILLSET_ID_BLANK_04:
        case SKILLSET_ID_MIMIC:
            battle_state_enter_after_command();
            break;
        case SKILLSET_ID_ATTACK:
        default:
            unit->command_state.ai = *command;
            unit->command_state.ai.data.action.actor_id = unit->stats->battle_id;
            unit->command_state.ai.data.action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
            battle_action_call_attack_preparation(&unit->command_state.ai.data.action);
            unit->used_ability_id = unit->command_state.ai.data.action.ability_id;
            result = battle_target_set_panels_for_action(&unit->command_state.ai.data.action);
            unit->ability_preview_phase = result;
            switch (result) {
            case TARGET_PANELS_UNIT_OR_TILE:
            case TARGET_PANELS_TILE_ONLY:
            case TARGET_PANELS_NONE:
                battle_state_enter_action_execution_setup();
                break;
            case TARGET_PANELS_NO_TARGETING:
            default:
                battle_target_select_tile();
                break;
            }
            break;
        }
        break;
    case AI_COMMAND_END_TURN:
        battle_action_choose_wait();
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
        battle_action_check_between_turn_events();
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
 * MenuToTargeting game-state handler for a unit the AI controls. From the
 * unit's 31st frame in the state it builds the unit's AI command, at half speed
 * until battle_ai_build_command reports the command ready (it can take several
 * frames), then carries the command out as the menu handler would: a move, an
 * act (Defend, Equip Change, Mimic, or an ability through target selection) or
 * the end of the turn, which marks both move and act taken.
 *
 * Each special skillset's completion stays explicit so its distinct call tail
 * is preserved.
 */
void battle_state_handle_menu_to_targeting_state(void) {
    battle_unit_t* unit;
    s32 kind;
    s32 result;
    u16 frame;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    unit = battle_unit_get_source();
    if (unit->state_frame_counter >= 31 && !unit->command_ready) {
        g_main_system_vsync_interval = 2;
        if (!battle_ai_build_command(unit->stats->battle_id, &unit->command_state.ai)) {
            if (unit->command_state.ai.kind == AI_COMMAND_ACT) {
                unit->command_state.ai.data.action.actor_id = unit->stats->battle_id;
                battle_action_call_attack_preparation(&unit->command_state.ai.data.action);
            }
            unit->command_ready = 1;
            g_main_system_vsync_interval = 1;
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
            battle_action_handle_move_command();
            return;
        }
        /* fallthrough */
    case AI_COMMAND_MOVE_SELF:
        g_battle_casting_runtime_id = g_battle_unit_source_runtime_id;
        battle_action_handle_move_command();
        return;
    case AI_COMMAND_ACT:
        switch (unit->command_state.ai.data.action.skillset_id) {
        case SKILLSET_ID_DEFEND:
            battle_status_inflict_defending_to_battle_id(unit->stats->battle_id);
            unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            battle_action_set_only_action_taken(unit->stats->battle_id);
            battle_state_enter_after_command();
            return;
        case SKILLSET_ID_EQUIP_CHANGE:
            unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
            battle_status_enable_disable_acting(unit->stats);
            battle_ai_init_unit_abilities(unit->stats->battle_id);
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            battle_action_set_only_action_taken(unit->stats->battle_id);
            battle_state_enter_after_command();
            return;
        case SKILLSET_ID_BLANK_04:
        case SKILLSET_ID_MIMIC:
            battle_action_set_only_action_taken(unit->stats->battle_id);
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
            result = battle_target_set_panels_for_action(&unit->command_state.ai.data.action);
            unit->ability_preview_phase = result;
            switch (result) {
            case TARGET_PANELS_UNIT_OR_TILE:
            case TARGET_PANELS_TILE_ONLY:
            case TARGET_PANELS_NONE:
                battle_state_enter_action_execution_setup();
                return;
            case TARGET_PANELS_NO_TARGETING:
            default:
                battle_target_select_tile();
                return;
            }
        }
    case AI_COMMAND_END_TURN:
        break;
    default:
        return;
    }
    unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_MOVED | BATTLE_UNIT_TURN_ACTION_ACTED;
    battle_action_end_turn(unit->stats->battle_id);
    battle_state_enter_after_command();
}

/* ActionHelpMenu game-state handler: when the help window has gone, reopen the
 * idle action menu, the active unit's when the casting unit is the source unit
 * and the non-active unit's otherwise. */
void battle_state_handle_action_help_menu_state(void) {
    battle_unit_t* unit;

    if (menu_get_thread_activity() != 2) {
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
    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* DisplayMoveArea game-state handler, a unit's move range shown from the free
 * cursor: cross clears the marks, restores the cursor input mode and returns to
 * the free cursor. */
void battle_state_handle_display_move_area_state(void) {
    if (g_main_input_pressed_buttons & PSX_PAD_CROSS) {
        battle_target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 1);
        g_battle_cursor_input_mode = g_battle_cursor_input_mode_saved;
        main_sound_play_sfx(MAIN_SFX_CANCEL);
        battle_state_set_free_cursor();
    }
    battle_camera_handle_rotation_input();
    battle_camera_update_zoom_input();
    battle_camera_update_tilt_input();
    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* MiniMenu game-state handler. Select opens the help window. A menu result of 0
 * (closed) restores the cursor movement option and returns to the free cursor; a
 * result of 0x64 or more shows the unit of AT-list entry result - 0x64. */
void battle_state_handle_mini_menu_state(void) {
    s32 menu_result = menu_get_thread_activity();

    if (g_main_input_pressed_buttons & PSX_PAD_SELECT) {
        battle_menu_open_mini_menu_help();
    } else if (menu_result == 0) {
        g_battle_cursor_input_mode = g_main_game_options.fields.cursor_movement;
        battle_state_set_free_cursor();
    } else if (menu_result >= 0x64) {
        battle_action_show_at_list_entry(menu_result - 0x64);
    }
    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* MiniMenuHelp game-state handler: when the help window has gone, reopen the mini
 * menu. */
void battle_state_handle_mini_menu_help_state(void) {
    if (menu_get_thread_activity() != 2) {
        g_battle_menu_help_opening = 0;
        battle_menu_open_free_cursor_mini_menu();
    }
    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* ActiveTurn game-state handler, the view of a unit picked from the AT list:
 * circle or cross returns to the mini menu. */
void battle_state_handle_active_turn_state(void) {
    s32 controller_input = g_main_input_pressed_buttons;

    if ((controller_input & PSX_PAD_CIRCLE) || (controller_input & PSX_PAD_CROSS)) {
        battle_menu_open_mini_menu();
    }
    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

/* UnitMove game-state handler: command 7 goes on to choosing a destination
 * (battle_state_enter_select_move_tile); 8 or 0xff returns to the idle action menu. */
void battle_state_handle_unit_move_state(void) {
    s32 selected_command;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    switch (selected_command) {
    case 7:
        battle_state_enter_select_move_tile();
        return;
    case 8:
    case 0xff:
        battle_menu_open_active_unit_idle_action_menu();
        return;
    }
}

/* CloseMoveHelp game-state handler: choosing the move destination. The casting
 * unit is the one that walks (the source unit, or its mount).
 *
 * A player-controlled source unit moves the cursor and camera. Circle paths the
 * casting unit to the cursor tile and starts the move setup, or opens the
 * illegal-move help when no path reaches it; cross clears the move range and
 * returns to the idle action menu; triangle recenters the cursor on the unit.
 * An AI-controlled unit waits 31 frames, then paths to the tile its AI command
 * chose, puts the cursor there and starts the move setup, or goes on to its next
 * scripted menu when no path reaches it. */
void battle_state_handle_close_move_help_state(void) {
    battle_unit_t* source = battle_unit_get_source();
    battle_unit_t* casting = battle_unit_get_casting();
    battle_walk_path_t* path;
    s32 buttons;
    u16 frame;
    s32 x;
    s32 elevation;
    s32 y;
    battle_unit_command_state_t scratch; /* never used; retail reserves its frame space */

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    if (source->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) {
        battle_target_move_cursor_by_input();
        battle_camera_handle_rotation_input();
        battle_camera_update_zoom_input();
        battle_camera_update_tilt_input();
        buttons = g_main_input_pressed_buttons;
        if (buttons & PSX_PAD_CIRCLE) {
            path = battle_move_build_path_to_tile(
                casting->stats->battle_id, g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
            if (path != 0) {
                casting->path = *path;
                casting->walk_speed = 0x2000;
                battle_target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 1);
                main_sound_play_sfx(MAIN_SFX_CONFIRM);
                battle_state_enter_unit_moving_setup();
                return;
            }
            battle_menu_open_illegal_move_help();
            return;
        }
        if (buttons & PSX_PAD_CROSS) {
            battle_target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 1);
            battle_target_move_cursor_to_unit(source);
            battle_menu_open_active_unit_idle_action_menu();
            return;
        }
        if (buttons & PSX_PAD_TRIANGLE) {
            battle_target_move_cursor_to_unit(battle_unit_get_source());
            battle_target_store_cursor_unit_name_and_data();
        }
        return;
    }
    frame = source->state_frame_counter;
    source->state_frame_counter = frame + 1;
    if (frame < 0x1f) {
        return;
    }
    path = battle_move_build_path_to_tile(casting->stats->battle_id, source->command_state.ai.data.move.x,
        source->command_state.ai.data.move.y, source->command_state.ai.data.move.layer);
    battle_target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 1);
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

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        battle_menu_dispatch_idle_action_menu();
    }
}

/* IllegalMoveMenu game-state handler: when the message closes (command 7, 8 or
 * 0xff), return to choosing a destination (battle_state_enter_select_move_tile). */
void battle_state_handle_illegal_move_menu_state(void) {
    s32 selected_command;

    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        battle_state_enter_select_move_tile();
    }
}

/* UnitMovingSetup game-state handler: command 7 starts the move; 8 or 0xff
 * discards the path and returns to choosing a destination. */
void battle_state_handle_unit_moving_setup_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    battle_unit_t* casting_unit;

    battle_state_update_units();
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
        battle_state_enter_select_move_tile();
        return;
    }
}

/* UnitMoving game-state handler: advance the casting unit along its path. When
 * the last step has ended and no animation is pending, refresh its display and
 * open the move confirmation. */
void battle_state_handle_unit_moving_state(void) {
    battle_unit_t* casting_unit;

    battle_camera_handle_rotation_input();
    battle_camera_update_zoom_input();
    battle_camera_update_tilt_input();
    casting_unit = battle_unit_get_casting();
    battle_move_update_path_step(casting_unit);
    if (casting_unit->step_phase == MOVE_STEP_PHASE_NONE
        && casting_unit->movement_path_offset >= casting_unit->path.count
        && g_battle_state_animation_continue_check == 0) {
        battle_unit_update_display_by_runtime_id(casting_unit->runtime_id);
        battle_menu_open_move_confirm();
    }
    battle_state_update_units();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}
