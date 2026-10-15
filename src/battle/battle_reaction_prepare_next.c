#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Reaction-ability outcome carried in `outcome` between the per-ability arms
 * and the common tail. */
enum {
    BATTLE_REACTION_OUTCOME_NONE = 0,
    BATTLE_REACTION_OUTCOME_COUNTER = 2,
};

/* Finds the first unit holding a pending reaction and resolves it.
 *
 * Walks the 21 unit slots, skipping the acting unit, and for the first unit
 * whose queued reaction id is set and who can still react, saves the unit's
 * action block, rebuilds it for the reaction and runs the matching preparation.
 * Returns the reacting unit's slot and writes its reaction ability id to
 * `out_ability`, or -1 when no unit reacts.
 *
 * The action pointer has its own register because stores use that base while
 * the unit pointer can change for Damage Split.
 */
s32 battle_reaction_prepare_next(s16* out_ability) {
    unit_stats_t* unit;
    action_command_t* action;
    s32 i;
    s32 outcome;
    s32 result;
    s32 reaction_prevented;
    u16 ability;
    u16 last_attack;
    s16 reaction_id;

    if (g_battle_acting_unit_id == -1) {
        return -1;
    }
    i = 0;
    do {
        g_action_context = ACTION_CONTEXT_PRIMARY;
        if (i != g_battle_acting_unit_id) {
            unit = &g_battle_unit_stats[i];
            action = &unit->action_command;
            if (battle_unit_get_action_restriction(unit) == BATTLE_UNIT_ACTION_RESTRICTION_NONE
                && unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                reaction_prevented = action_is_reaction_prevented(unit);
                ability = unit->action.reaction_id;
                if (((reaction_prevented == 0 && g_action_state != ACTION_STATE_AI_SIMULATION)
                        || ability == ABILITY_ID_REACTION_REFLECT)
                    && ability != 0) {
                    g_action_context = ACTION_CONTEXT_REACTION_OR_SIMULATION;
                    main_util_copy_action_data(action, &g_action_saved_command);
                    g_battle_current_reaction_ability_id = ability;
                    action->actor_id = i;
                    action->skillset_id = 0;
                    reaction_id = g_battle_current_reaction_ability_id;
                    outcome = 1;
                    action->ability_id = reaction_id;
                    last_attack = unit->action.last_received_attack;
                    unit->action.reaction_id = 0;
                    switch (reaction_id) {
                    case ABILITY_ID_REACTION_REFLECT:
                        outcome = -(target_apply_reflect(unit) == 0) & 3;
                        break;
                    case ABILITY_ID_REACTION_MEATBONE_SLASH:
                        if (action_store_counter_ability(unit, SKILLSET_ID_ATTACK, 0, 1) != 0) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        } else {
                            action->skillset_id = 1;
                            action->targeting_type = 5;
                        }
                        break;
                    case ABILITY_ID_REACTION_COUNTER_MAGIC:
                        outcome
                            = (action_store_counter_ability(unit, SKILLSET_ID_BLACK_MAGIC, last_attack, 0) == 0) * 2;
                        break;
                    case ABILITY_ID_REACTION_COUNTER_TACKLE:
                        if (action_store_counter_ability(unit, SKILLSET_ID_BLACK_MAGIC, ABILITY_ID_BASIC_SKILL_DASH, 1)
                            != 0) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        }
                        break;
                    case ABILITY_ID_REACTION_COUNTER_FLOOD:
                        outcome = (action_store_counter_ability(unit, SKILLSET_ID_ELEMENTAL,
                                       g_main_terrain_geomancy_abilities[g_map_tile_data[map_calculate_location(unit)]
                                               .surface.type],
                                       0)
                                      == 0)
                            * 2;
                        break;
                    case ABILITY_ID_REACTION_AUTO_POTION:
                        result = action_select_auto_potion_item(unit);
                        if (result == -1) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                            break;
                        }
                        action->skillset_id = 6;
                        /* Auto-Potion updates only the low byte of the 16-bit item/equipment field. */
                        *(u8*)&action->item_or_equipment = result;
                        action->targeting_type = 5;
                        action->target_id = i;
                        action->target_position.vx = unit->x;
                        action->target_position.vz = unit->position.y;
                        action->target_position.vy = unit->position.higher_elevation;
                        result = action_call_attack_preparation_at_preview(action);
                        outcome = BATTLE_REACTION_OUTCOME_COUNTER;
                        unit->action_command.ability_id = ABILITY_ID_REACTION_AUTO_POTION;
                        if (result == -1 || result == 2 || result == 3) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        }
                        break;
                    case ABILITY_ID_REACTION_COUNTER:
                        if (action_store_counter_ability(unit, SKILLSET_ID_ATTACK, 0, 1) != 0) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        }
                        break;
                    default:
                        outcome
                            = -((g_main_reaction_behavior_flags[g_battle_current_reaction_ability_id
                                     - ABILITY_ID_REACTION_FIRST]
                                    & (BATTLE_REACTION_BEHAVIOR_FLAG_PASSIVE | BATTLE_REACTION_BEHAVIOR_FLAG_ACTIVE))
                                == 0);
                        break;
                    }
                    if ((outcome == 1 || outcome == 2) && action_check_and_consume_mp(unit) != 0) {
                        outcome = BATTLE_REACTION_OUTCOME_NONE;
                    }
                    if (outcome == BATTLE_REACTION_OUTCOME_NONE) {
                        main_util_copy_action_data(&g_action_saved_command, action);
                        i++;
                        continue;
                    }
                    if (outcome >= 1 && outcome <= 3) {
                        action_init_current_ability_strike_data(unit);
                    } else {
                        g_action_current_ability.strike_count = 1;
                        g_action_current_ability.strike_counter = 0;
                        g_action_current_ability.weapon_proc_pending = 0;
                        g_action_current_ability.knockback_flags = 0;
                        g_action_current_ability.primary_weapon_id
                            = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
                        g_action_current_ability.secondary_weapon_id
                            = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
                        action->skillset_id = 0;
                        action->targeting_type = 5;
                        action->target_id = i;
                        if (g_battle_current_reaction_ability_id == ABILITY_ID_REACTION_DAMAGE_SPLIT) {
                            action->target_id = g_battle_acting_unit_id;
                            unit = &g_battle_unit_stats[g_battle_acting_unit_id];
                        }
                        action->target_position.vx = unit->x;
                        action->target_position.vz = unit->position.y;
                        action->target_position.vy = unit->position.higher_elevation;
                    }
                    *out_ability = g_battle_current_reaction_ability_id;
                    action->actor_id = i;
                    return i;
                }
            }
        }
        i++;
    } while (i < 21);
    g_action_context = ACTION_CONTEXT_PRIMARY;
    *out_ability = 0;
    return -1;
}
