/* Action announcements, pre-attack animation, casting, target selection and display, vsync interval changes,
 * and the state handlers from the free cursor through unit moving. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/ai.h"
#include "fft/ai_command.h"
#include "fft/animation.h"
#include "fft/at_list.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/cursor.h"
#include "fft/effect.h"
#include "fft/effect_script.h"
#include "fft/fade.h"
#include "fft/gfx.h"
#include "fft/help.h"
#include "fft/input.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/message.h"
#include "fft/move.h"
#include "fft/move_step.h"
#include "fft/option.h"
#include "fft/reaction.h"
#include "fft/result.h"
#include "fft/skillset.h"
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
#include "fft/vector.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

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
static void effect_start_for_action(s32 ability, unit_t* unit) {
    effect_context_t init;
    unit_t* target;
    unit_t* spectator;
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
    vector_set_svector(
        &init.target_tile, unit->strike.target_tile.x, unit->strike.target_tile.layer, unit->strike.target_tile.y);
    block = &unit->strike;
    if (init.target_count != 0) {
        for (i = 0; i < init.target_count; i++) {
            target = unit_get_by_battle_id(block->target_list[i]);
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
            spectator = unit_get_by_runtime_id(i);
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
    effect_set_ability_animation(packed, ability, &init);
}

/* Announce the next action that comes due: a Hamedo, the acting unit's own action or a reaction.
 *
 * Each of the three action phases is polled in turn; a phase that produces a
 * unit id publishes the announcement state, resolves the spell quote and stops
 * the scan.  Returns 1 once an announcement was set up, 0 when all three phases
 * had nothing pending. */
s32 action_announce_next(void) {
    action_command_t command;
    unit_t* unit = unit_get_source();
    unit_t* actor;
    s32 result;
    s32 ability_id;
    unit_stats_t* actor_stats;

    do {
        switch (g_action_phase) {
        case ACTION_PHASE_PRE_ACTION:
            result = reaction_prepare_hamedo(unit->stats->battle_id, &command.ability_id);
            if (result != -1) {
                g_game_state = GAME_STATE_ACTION_ANNOUNCEMENT;
                actor = unit_get_by_battle_id(result);
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
                ability_id = unit->command_state.ai.data.action.ability_id;
                unit->continue_attack_count = 0;
                g_state_window_closed = 0;
                unit->sp2_ability_id = ability_id;
                unit->used_ability_id = ability_id;
            } else {
                if (result >= 0) {
                    if (result < 4) {
                        result_queue_charge_failure_message(unit, result);
                    }
                }
                animation_set_for_status(unit);
                action_end_command(unit);
                result = -1;
            }
            break;
        case ACTION_PHASE_REACTING_UNIT:
            result = reaction_prepare_next(&command.ability_id);
            if (result != -1) {
                g_game_state = GAME_STATE_ACTION_ANNOUNCEMENT;
                actor = unit_get_by_battle_id(result);
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
    unit_t* unit;
    unit_t* target;
    action_strike_work_t* action;
    s32 i;

    g_system_vsync_interval = 1;
    g_game_state = GAME_STATE_ACTION_EXECUTE_START;
    unit = unit_get_casting();
    unit->state_frame_counter = 0;
    if (unit->continue_attack_count == 0) {
        unit_facing_turn_to_target(unit, 0);
    }
    action = &unit->strike;
    if (action->target_count != 0) {
        for (i = 0; i < action->target_count; i++) {
            target = unit_get_by_battle_id(action->target_list[i]);
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
            animation_select_attack_for_ability(unit, unit_get_by_battle_id(action->target_list[0]));
        } else if (unit->used_ability_id == 0 || IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)) {
            if (action->target_count != 0) {
                animation_select_weapon_attack(unit, unit_get_by_battle_id(action->target_list[0]));
            } else {
                animation_select_weapon_attack(unit, 0);
            }
        }
    } else if (unit->used_ability_id == ABILITY_ID_KNOCKBACK) {
        for (i = 0; i < unit->strike.target_count; i++) {
            target = unit_get_by_battle_id(action->target_list[i]);
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
                animation_select_attack_for_ability(unit, unit_get_by_battle_id(action->target_list[0]));
            }
        } else if (unit->used_ability_id == 0 || IS_ELEMENTAL_GUN(unit->used_item_or_weapon_id)) {
            if (action->target_count != 0) {
                animation_select_weapon_attack(unit, unit_get_by_battle_id(action->target_list[0]));
            } else {
                animation_select_weapon_attack(unit, 0);
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
    unit_t* unit;
    u16 attack_id;
    s32 ability;
    s32 reaction;

    g_system_vsync_interval = 1;
    unit = unit_get_casting();
    action_resolve_strike(unit->stats->battle_id, &unit->strike);
    attack_id = unit->strike.last_attack_id;
    /* The target reads reaction_id as a signed 16-bit value. */
    reaction = unit->strike.reaction_id;
    ability = attack_id;
    unit->used_ability_id = attack_id;
    sprite_palette_set_thrown_item(unit->strike.used_weapon_id, unit);
    if (((reaction != 0) && (ability != reaction)) || (ability == ABILITY_ID_KNOCKBACK) || (ability == 0)
        || (effect_ability_has_no_file(ability) != 0)) {
        action_apply_strike_results();
        return;
    }
    g_game_state = GAME_STATE_START_EFFECT_FILE_OPEN;
    effect_start_for_action(ability, unit);
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
    unit_t* unit;
    u16 ability;

    target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 2);
    g_game_state = GAME_STATE_PRE_ATTACK_ANIMATION;
    unit = unit_get_source();
    ability = unit->used_ability_id;
    unit->state_frame_counter = 0;
    if (g_ability_animation_data[ability].charge_animation_set_id != 0) {
        animation_start_ability_charge(unit);
    } else {
        /* facing is read signed (lh) here. */
        animation_store_facing(ANIMATION_STAND, unit->facing, unit);
    }
    cursor_hide();
}

/* Enter the action-cast state for the source unit, unless the scenario check
 * starts an event first; the camera turns when the unit's tile is hidden. */
void state_enter_action_cast(void) {
    unit_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_ACTION_CAST;
    g_state_window_closed = 0;
    unit = unit_get_source();
    if (event_try_enter_scenario(unit) == 0) {
        g_game_state = GAME_STATE_ACTION_CAST;
        camera_rotate_when_unit_tile_not_visible(unit);
    }
}

/* Starts the acting unit's confirmed ability: marks the action taken, faces the
 * cursor and commits the attack preparation. An immediate or Jump action enters
 * the commence-attack phase, with the charge-hold animation when the ability has a
 * charge set. A charged action starts its charge-hold animation and ends the
 * command until the charge comes due; any other result raises a pointer
 * exception and ends the command. */
void action_execute_ability(void) {
    unit_t* unit;
    s32 result;

    unit_view_start(UNIT_VIEW_MODE_NONE, 0, 0);
    unit = unit_get_source();
    turn_set_act_taken(unit->stats->battle_id);
    unit->turn_action_flags |= TURN_ACTION_ACTED;
    unit_facing_turn_to_cursor(unit);
    result = action_commit_preparation(&unit->command_state.ai.data.action);
    unit->attack_phase_state = result;
    switch (result) {
    case ACTION_PREPARATION_CHARGE:
        animation_start_ability_charge_hold(unit);
        break;
    case ACTION_PREPARATION_IMMEDIATE:
    case ACTION_PREPARATION_JUMP:
        camera_pan_disable();
        g_game_state = GAME_STATE_COMMENCE_ATTACK_PHASE;
        g_state_window_closed = 0;
        unit->turn_action_flags |= TURN_ACTION_ACTED;
        if (g_ability_animation_data[unit->used_ability_id].charge_animation_set_id != 0) {
            animation_start_ability_charge_hold(unit);
        }
        return;
    case ACTION_PREPARATION_CHANGE_EQUIPMENT:
    default:
        system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_ATTACK_PREPARATION);
        break;
    }
    action_end_command(unit);
}

/* Enter the commence-attack phase for the source unit: copy the action command stored in its
 * battle stats into its runtime action, mark it acted and start the charge-hold animation when it
 * uses an ability. */
void state_enter_commence_attack_phase(void) {
    unit_stats_t* stats;
    unit_t* unit;

    camera_pan_disable();
    g_game_state = GAME_STATE_COMMENCE_ATTACK_PHASE;
    unit = unit_get_source();
    stats = unit->stats;
    unit->command_state.ai.data.action = stats->action_command;
    /* action_command.ability_id is s16 in the header; the target loads it with lhu. */
    unit->used_ability_id = unit->stats->action_command.ability_id;
    unit->turn_action_flags |= TURN_ACTION_ACTED;
    if (unit->used_ability_id != 0) {
        animation_start_ability_charge_hold(unit);
    }
}

/* Enter the ability-preview-help state and start the help menu. Nothing in the
 * game calls it. */
void state_enter_ability_preview_help(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_ABILITY_PREVIEW_HELP;
    g_state_in_help = 1;
    help_start_menu_thread();
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
void target_select_tile(void) {
    unit_t* unit = unit_get_source();
    s32 result;
    s32 saved_input_mode;

    unit->state_frame_counter = 0;
    if (unit->ability_preview_phase == TARGET_PANELS_NO_TARGETING) {
        vector_set_svector(
            &unit->command_state.ai.data.action.target_position, unit->map_x, unit->map_layer, unit->map_y);
        unit->command_state.ai.data.action.targeting_type = TARGET_TYPE_TILE;
    } else if (unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) {
        vector_set_svector(&unit->command_state.ai.data.action.target_position, g_cursor_x, g_cursor_layer, g_cursor_y);
        unit->command_state.ai.data.action.targeting_type = TARGET_TYPE_TILE;
    }
    result = target_area_set_for_action(&unit->command_state.ai.data.action);
    unit->target_select_result = result;
    switch (result) {
    case 0:
    case 1:
        camera_pan_enable();
        saved_input_mode = g_cursor_input_mode;
        g_game_state = GAME_STATE_ABILITY_PREVIEW_HANDLING;
        g_cursor_input_mode = CURSOR_INPUT_FREE_CURSOR;
        g_cursor_input_mode_saved = saved_input_mode;
        target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_SHOW_TARGETS, 3);
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
    unit_t* unit;
    unit_stats_t* stats;

    camera_pan_disable();
    g_game_state = GAME_STATE_ILLEGAL_RANGE;
    unit = unit_get_source();
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
target_panels_e target_begin_tile_selection(void) {
    unit_t* unit = unit_get_source();

    unit->state_frame_counter = 0;
    switch (unit->ability_preview_phase) {
    case TARGET_PANELS_UNIT_OR_TILE:
    case TARGET_PANELS_TILE_ONLY:
        camera_pan_disable();
        g_game_state = GAME_STATE_TARGETING_RANGE;
        target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_SHOW_RANGE, 2);
        cursor_show_right_unit_panel();
        if ((unit->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
            cursor_show();
        } else {
            cursor_hide();
        }
        break;
    case TARGET_PANELS_NO_TARGETING:
        camera_pan_disable();
        target_select_tile();
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
    unit_t* unit;
    unit_t* target;

    camera_pan_disable();
    g_game_state = GAME_STATE_CONFIRM_ACTION;
    unit = unit_get_source();
    target = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
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
 * AI_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT, blue on every unit of its own
 * team for AI_COMMAND_AUTOBATTLE_PROTECT_ALLIES_SELECT. Other modes tint
 * nothing. */
static void sprite_palette_highlight_blue_or_red(s32 mode) {
    unit_t* caster = unit_get_casting();
    unit_t* unit;
    s32 i;

    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        unit = unit_get_by_runtime_id(i);
        if (unit == 0) {
            continue;
        }
        if (unit->stats == 0) {
            continue;
        }
        switch (mode) {
        case AI_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT:
            if ((unit->stats->team_flags & UNIT_TEAM_MASK) != (caster->stats->team_flags & UNIT_TEAM_MASK)) {
                battle_map_palette_modify(
                    BATTLE_MAP_COLOR_MODE_BASE_REPEAT, 4, BATTLE_MAP_CLUT_ROW_UNITS, i, 0, 0x10, 0, 0);
            }
            continue;
        case AI_COMMAND_AUTOBATTLE_PROTECT_ALLIES_SELECT:
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
static void state_enter_target_select_start(void) {
    unit_t* unit;
    unit_stats_t* stats;

    camera_pan_disable();
    g_game_state = GAME_STATE_TARGET_SELECT_START;
    unit = unit_get_casting();
    stats = unit->stats;
    menu_init_system_function(
        MENU_SYSTEM_COMMAND_TARGETING_PROMPT, MENU_TARGETING_PROMPT_DEFAULT, stats->battle_id, 0, 1);
    cursor_show();
}

/* Enter the target-select state: show the panel for the cursor tile and tint the
 * units the casting unit's selection command may pick, the other teams' red for a
 * targeted selection and its own team's blue for a recovery selection. */
void state_enter_target_select(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_TARGET_SELECT;
    cursor_show_hovered_unit_panel();
    sprite_palette_highlight_blue_or_red(unit_get_casting()->target_select_command);
}

/* Enter the target-select-denied state for the cursor tile: show the summary of a
 * unit standing there and open the refusal variant of the target-confirm prompt
 * for the casting unit.
 *
 * Option 3 is used for an empty tile, 1 for a targeted auto-battle selection and
 * 2 for a recovery selection. Any other selection command over a unit opens no
 * prompt. */
void state_enter_target_select_denied(void) {
    unit_t* occupant;
    unit_t* caster;
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
    caster = unit_get_casting();
    if (occupant == 0) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 3, caster->stats->battle_id, 0, 1);
        return;
    }
    command = caster->target_select_command;
    if (command == AI_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 1, caster->stats->battle_id, 0, 1);
        return;
    }
    if (command == AI_COMMAND_AUTOBATTLE_PROTECT_ALLIES_SELECT) {
        menu_init_system_function(MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 2, caster->stats->battle_id, 0, 1);
    }
}

/* Enter the target-select-confirm state: show the summary of the unit under the
 * cursor and open the target-confirm prompt (option 0) for the casting unit. */
void state_enter_target_select_confirm(void) {
    unit_t* unit;
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
    stats = unit_get_casting()->stats;
    menu_init_system_function(MENU_SYSTEM_COMMAND_TARGET_CONFIRM, 0, stats->battle_id, 0, 1);
}

/* Enter the target-display-start state: show the tile info panel and open the
 * targeting prompt for the casting unit. */
static void state_enter_target_display_start(void) {
    camera_pan_disable();
    g_game_state = GAME_STATE_TARGET_DISPLAY_START;
    unit_view_start(UNIT_VIEW_MODE_TILE_INFO, 0xFF, 0xFF);
    menu_init_system_function(MENU_SYSTEM_COMMAND_TARGETING_PROMPT, MENU_TARGETING_PROMPT_DEFAULT,
        unit_get_casting()->stats->battle_id, 0, 1);
    g_state_window_closed = 0;
    cursor_show();
}

/* Enter the target-display state: move the cursor to the casting unit's
 * auto-battle target, turn the camera when its tile is hidden and show its
 * summary panel. */
void state_enter_target_display(void) {
    unit_stats_t* caster_stats;
    unit_t* target;
    unit_stats_t* target_stats;

    camera_pan_disable();
    g_game_state = GAME_STATE_TARGET_DISPLAY;
    caster_stats = unit_get_casting()->stats;
    target = unit_get_by_battle_id(caster_stats->auto_battle_target);
    cursor_move_to_unit(target);
    camera_rotate_when_unit_tile_not_visible(target);
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
        cursor_draw_with_tile_glow();
        return;
    }
    cursor_project_tile_to_screen(&projected, &position, &raised);
}

/* Per-frame unit update shared by most game-state handlers; it reads no input.
 *
 * Projects the units, animates them unless the status screen owns the frame,
 * updates the status bubbles and unit graphics, and projects the cursor tile,
 * drawing the map cursor and tile glow while g_cursor_visible is
 * set. */
void state_update_units(void) {
    sprite_project_all_units();
    if (g_menu_status_screen_selected != 1) {
        unit_update_and_animate_all();
    }
    sprite_update_status_bubbles_and_graphics();
    cursor_update();
}

/* Free-cursor game state: camera and cursor input, then the button actions
 * (help, team highlight, act menu, move range, AT list and mini menu). */
void state_handle_free_cursor(void) {
    unit_t* unit;
    s32 input;
    RECT rect; /* never used; retail reserves its 8 bytes */

    camera_rotation_handle_input();
    camera_zoom_handle_input();
    camera_tilt_handle_input();
    cursor_move_by_input();
    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    input = g_input_pressed_buttons;
    if (input & PSX_PAD_SELECT) {
        at_list_reset_cursor();
        state_enter_free_cursor_help();
    } else if (input & PSX_PAD_SQUARE) {
        at_list_reset_cursor();
        state_enter_highlight_units();
    } else if (input & PSX_PAD_CIRCLE) {
        target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 0);
        sound_play_sfx(SOUND_SFX_CONFIRM);
        if (unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer) != 0) {
            menu_action_dispatch();
        } else {
            unit = unit_get_source();
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
            sound_play_sfx(SOUND_SFX_CONFIRM);
            at_list_show_next_entry();
            cursor_show_hovered_unit_panel();
        }
    } else if (input & PSX_PAD_TRIANGLE) {
        at_list_reset_cursor();
        sound_play_sfx(SOUND_SFX_CONFIRM);
        state_enter_mini_menu();
    }
}

/* FreeCursorHelp game-state handler: when the help window has gone, return to the
 * free cursor. */
void state_handle_free_cursor_help(void) {
    if (menu_get_thread_activity() != 2) {
        g_state_in_help = 0;
        state_enter_free_cursor();
    }
    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
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
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
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
    ai_command_t* command;
    unit_t* unit;
    s32 result;

    if (menu_get_thread_activity() == 0) {
        state_enter_free_cursor();
    }
    if (g_input_pressed_buttons & PSX_PAD_SELECT) {
        state_enter_action_help_menu();
    }
    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    command = command_get_selected();
    unit = unit_get_casting();
    switch (command->kind) {
    case AI_COMMAND_MOVE:
        if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
            g_unit_casting_runtime_id = unit_get_by_runtime_id(unit->mount_partner_runtime_id)->runtime_id;
        } else {
            g_unit_casting_runtime_id = g_unit_source_runtime_id;
        }
        at_list_reset_cursor();
        state_enter_unit_move();
        break;
    case AI_COMMAND_MOVE_SELF:
        g_unit_casting_runtime_id = g_unit_source_runtime_id;
        at_list_reset_cursor();
        state_enter_unit_move();
        break;
    case AI_COMMAND_ACT:
        at_list_reset_cursor();
        switch (command->data.action.skillset_id) {
        case SKILLSET_ID_DEFEND:
            unit->command_state.ai = *command;
            status_inflict_defending(unit->stats->battle_id);
            unit->turn_action_flags |= TURN_ACTION_ACTED;
            unit_update_display_by_runtime_id(unit->runtime_id);
            turn_set_act_taken(unit->stats->battle_id);
            state_enter_after_command();
            break;
        case SKILLSET_ID_EQUIP_CHANGE:
            unit->command_state.ai = *command;
            unit->turn_action_flags |= TURN_ACTION_ACTED;
            status_clear_action_state(unit->stats);
            ai_init_unit_abilities(unit->stats->battle_id);
            unit_update_display_by_runtime_id(unit->runtime_id);
            turn_set_act_taken(unit->stats->battle_id);
        case SKILLSET_ID_BLANK_04:
        case SKILLSET_ID_MIMIC:
            state_enter_after_command();
            break;
        case SKILLSET_ID_ATTACK:
        default:
            unit->command_state.ai = *command;
            unit->command_state.ai.data.action.actor_id = unit->stats->battle_id;
            unit->command_state.ai.data.action.targeting_type = TARGET_TYPE_TILE;
            action_prepare_in_place(&unit->command_state.ai.data.action);
            unit->used_ability_id = unit->command_state.ai.data.action.ability_id;
            result = target_range_set_for_action(&unit->command_state.ai.data.action);
            unit->ability_preview_phase = result;
            switch (result) {
            case TARGET_PANELS_UNIT_OR_TILE:
            case TARGET_PANELS_TILE_ONLY:
            case TARGET_PANELS_NONE:
                state_enter_targeting_prompt();
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
        state_enter_wait_menu();
        break;
    case AI_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT:
    case AI_COMMAND_AUTOBATTLE_PROTECT_ALLIES_SELECT:
        unit->target_select_command = command->kind;
        state_enter_target_select_start();
        break;
    case AI_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_DISPLAY:
    case AI_COMMAND_AUTOBATTLE_PROTECT_ALLIES_DISPLAY:
        unit->target_select_command = command->kind;
        state_enter_target_display_start();
        break;
    case AI_COMMAND_AUTOBATTLE_FADING_LIFE:
    case AI_COMMAND_AUTOBATTLE_RETREAT:
        state_enter_open_action_menus();
        break;
    case AI_COMMAND_NO_MENU_RECORD:
        menu_status_clear();
        turn_start_next_event();
        break;
    case AI_COMMAND_REFRESH_CONTROL:
        unit_store_team_flags(unit);
        break;
    case AI_COMMAND_UNRESOLVED_FE:
    case AI_COMMAND_UNRESOLVED_FF:
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
    unit_t* unit;
    s32 kind;
    s32 result;
    u16 frame;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    unit = unit_get_source();
    if (unit->state_frame_counter >= 31 && !unit->command_ready) {
        g_system_vsync_interval = 2;
        if (!ai_command_build(unit->stats->battle_id, &unit->command_state.ai)) {
            if (unit->command_state.ai.kind == AI_COMMAND_ACT) {
                unit->command_state.ai.data.action.actor_id = unit->stats->battle_id;
                action_prepare_in_place(&unit->command_state.ai.data.action);
            }
            unit->command_ready = 1;
            g_system_vsync_interval = 1;
        }
    }
    frame = unit->state_frame_counter++;
    if (frame < 31 || !unit->command_ready)
        return;
    kind = unit->command_state.ai.kind;
    switch (kind) {
    case AI_COMMAND_MOVE:
        if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
            g_unit_casting_runtime_id = unit_get_by_runtime_id(unit->mount_partner_runtime_id)->runtime_id;
            state_enter_unit_move();
            return;
        }
        /* fallthrough */
    case AI_COMMAND_MOVE_SELF:
        g_unit_casting_runtime_id = g_unit_source_runtime_id;
        state_enter_unit_move();
        return;
    case AI_COMMAND_ACT:
        switch (unit->command_state.ai.data.action.skillset_id) {
        case SKILLSET_ID_DEFEND:
            status_inflict_defending(unit->stats->battle_id);
            unit->turn_action_flags |= TURN_ACTION_ACTED;
            unit_update_display_by_runtime_id(unit->runtime_id);
            turn_set_act_taken(unit->stats->battle_id);
            state_enter_after_command();
            return;
        case SKILLSET_ID_EQUIP_CHANGE:
            unit->turn_action_flags |= TURN_ACTION_ACTED;
            status_clear_action_state(unit->stats);
            ai_init_unit_abilities(unit->stats->battle_id);
            unit_update_display_by_runtime_id(unit->runtime_id);
            turn_set_act_taken(unit->stats->battle_id);
            state_enter_after_command();
            return;
        case SKILLSET_ID_BLANK_04:
        case SKILLSET_ID_MIMIC:
            turn_set_act_taken(unit->stats->battle_id);
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
            unit->used_ability_id = unit->command_state.ai.data.action.ability_id;
            result = target_range_set_for_action(&unit->command_state.ai.data.action);
            unit->ability_preview_phase = result;
            switch (result) {
            case TARGET_PANELS_UNIT_OR_TILE:
            case TARGET_PANELS_TILE_ONLY:
            case TARGET_PANELS_NONE:
                state_enter_targeting_prompt();
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
    unit->turn_action_flags |= TURN_ACTION_MOVED | TURN_ACTION_ACTED;
    turn_end(unit->stats->battle_id);
    state_enter_after_command();
}

/* ActionHelpMenu game-state handler: when the help window has gone, reopen the
 * idle action menu, the active unit's when the casting unit is the source unit
 * and the non-active unit's otherwise. */
void state_handle_action_help_menu(void) {
    unit_t* unit;

    if (menu_get_thread_activity() != 2) {
        g_state_in_help = 0;
        unit = unit_get_casting();
        if (unit != 0) {
            if (unit->runtime_id == g_unit_source_runtime_id) {
                menu_action_open_active_unit();
            } else {
                menu_action_open_non_active_unit();
            }
        }
    }
    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
}

/* DisplayMoveArea game-state handler, a unit's move range shown from the free
 * cursor: cross clears the marks, restores the cursor input mode and returns to
 * the free cursor. */
void state_handle_display_move_area(void) {
    if (g_input_pressed_buttons & PSX_PAD_CROSS) {
        target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 1);
        g_cursor_input_mode = g_cursor_input_mode_saved;
        sound_play_sfx(SOUND_SFX_CANCEL);
        state_enter_free_cursor();
    }
    camera_rotation_handle_input();
    camera_zoom_handle_input();
    camera_tilt_handle_input();
    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
}

/* MiniMenu game-state handler. Select opens the help window. A menu result of 0
 * (closed) restores the cursor movement option and returns to the free cursor; a
 * result of 0x64 or more shows the unit of AT-list entry result - 0x64. */
void state_handle_mini_menu(void) {
    s32 menu_result = menu_get_thread_activity();

    if (g_input_pressed_buttons & PSX_PAD_SELECT) {
        state_enter_mini_menu_help();
    } else if (menu_result == 0) {
        g_cursor_input_mode = g_game_options.fields.cursor_movement;
        state_enter_free_cursor();
    } else if (menu_result >= 0x64) {
        at_list_show_entry(menu_result - 0x64);
    }
    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
}

/* MiniMenuHelp game-state handler: when the help window has gone, reopen the mini
 * menu. */
void state_handle_mini_menu_help(void) {
    if (menu_get_thread_activity() != 2) {
        g_state_in_help = 0;
        state_enter_mini_menu();
    }
    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
}

/* Inspect the unit or charged action selected from the AT List.
 * Circle or Cross returns to the mini menu. */
void state_handle_at_list_inspection(void) {
    s32 controller_input = g_input_pressed_buttons;

    if ((controller_input & PSX_PAD_CIRCLE) || (controller_input & PSX_PAD_CROSS)) {
        state_return_to_mini_menu();
    }
    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
}

/* UnitMove game-state handler: YES goes on to choosing a destination
 * (state_enter_select_move_tile); NO or UNRESOLVED_FF returns to the idle action menu. */
void state_handle_unit_move(void) {
    s32 selected_command;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    switch (selected_command) {
    case AI_COMMAND_PROMPT_YES:
        state_enter_select_move_tile();
        return;
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
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
    unit_t* source = unit_get_source();
    unit_t* casting = unit_get_casting();
    move_path_t* path;
    s32 buttons;
    u16 frame;
    unit_command_state_t scratch; /* never used; retail reserves its frame space */

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
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
                target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 1);
                sound_play_sfx(SOUND_SFX_CONFIRM);
                state_enter_unit_moving_setup();
                return;
            }
            state_enter_illegal_move_menu();
            return;
        }
        if (buttons & PSX_PAD_CROSS) {
            target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 1);
            cursor_move_to_unit(source);
            menu_action_open_active_unit();
            return;
        }
        if (buttons & PSX_PAD_TRIANGLE) {
            cursor_move_to_unit(unit_get_source());
            cursor_show_hovered_unit_panel();
        }
        return;
    }
    frame = source->state_frame_counter;
    source->state_frame_counter = frame + 1;
    if (frame < 0x1f) {
        return;
    }
    path = move_path_build_to_tile(casting->stats->battle_id, source->command_state.ai.data.move.x,
        source->command_state.ai.data.move.y, source->command_state.ai.data.move.layer);
    target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 1);
    if (path != 0) {
        g_cursor_x = source->command_state.ai.data.move.x;
        g_cursor_layer = source->command_state.ai.data.move.layer;
        g_cursor_y = source->command_state.ai.data.move.y;
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
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        menu_action_dispatch();
    }
}

/* IllegalMoveMenu game-state handler: when the message closes (any answer), return to choosing a
 * destination (state_enter_select_move_tile). */
void state_handle_illegal_move_menu(void) {
    s32 selected_command;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command = command_get_selected()->kind;
    if (selected_command >= AI_COMMAND_PROMPT_YES
        && (selected_command <= AI_COMMAND_PROMPT_NO || selected_command == AI_COMMAND_UNRESOLVED_FF)) {
        state_enter_select_move_tile();
    }
}

/* UnitMovingSetup game-state handler: YES starts the move; NO or UNRESOLVED_FF discards the path
 * and returns to choosing a destination. */
void state_handle_unit_moving_setup(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    unit_t* casting_unit;

    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
    selected_command_address = command_get_selected();
    casting_unit = unit_get_casting();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case AI_COMMAND_PROMPT_YES:
        state_enter_unit_moving();
        return;
    case AI_COMMAND_PROMPT_NO:
    case AI_COMMAND_UNRESOLVED_FF:
        casting_unit->path.count = 0;
        state_enter_select_move_tile();
        return;
    }
}

/* UnitMoving game-state handler: advance the casting unit along its path. When
 * the last step has ended and no animation is pending, refresh its display and
 * open the move confirmation. */
void state_handle_unit_moving(void) {
    unit_t* casting_unit;

    camera_rotation_handle_input();
    camera_zoom_handle_input();
    camera_tilt_handle_input();
    casting_unit = unit_get_casting();
    move_step_update(casting_unit);
    if (casting_unit->step_phase == MOVE_STEP_PHASE_NONE
        && casting_unit->movement_path_offset >= casting_unit->path.count && g_state_animation_continue_check == 0) {
        unit_update_display_by_runtime_id(casting_unit->runtime_id);
        state_enter_move_confirm_menu();
    }
    state_update_units();
    menu_update_menus_and_event_speed(gfx_get_battle_otag(), g_input_current_buttons);
}
