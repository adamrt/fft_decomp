#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/map.h"
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
 * The action block is addressed through a `u8*` at battle_stats_t +0x16e for
 * the same reason as battle_action_store_counter_ability: the target keeps that
 * pointer in its own register and stores through it rather than through the
 * unit.
 */
s32 battle_reaction_prepare_next(u16* out_ability) {
    battle_stats_t* unit;
    u8* action;
    s32 i;
    s32 outcome;
    s32 result;
    s32 can_react;
    u16 ability;
    u16 last_attack;
    s32 reaction_id;

    if (g_battle_acting_unit_id == -1) {
        return -1;
    }
    i = 0;
    do {
        g_battle_action_context = 0;
        if (i != g_battle_acting_unit_id) {
            unit = &g_battle_unit_stats[i];
            action = &unit->action_actor_id;
            if (battle_formula_can_unit_evade(unit) == 0 && unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
                can_react = battle_action_can_unit_react(unit);
                ability = unit->action.reaction_id;
                if (((can_react == 0 && g_battle_action_state != 1) || ability == ABILITY_ID_REACTION_REFLECT)
                    && ability != 0) {
                    g_battle_action_context = 1;
                    main_util_copy_action_data(action, g_reaction_unit_action_data_16e);
                    g_battle_current_reaction_ability_id = ability;
                    action[0] = i;
                    action[1] = 0;
                    reaction_id = *(u16*)&g_battle_current_reaction_ability_id;
                    outcome = 1;
                    *(u16*)(action + 2) = reaction_id;
                    last_attack = unit->action.last_received_attack;
                    unit->action.reaction_id = 0;
                    switch ((s16)(reaction_id - 0x1B2)) {
                    case 6:
                        outcome = -(battle_target_apply_reflect(unit) == 0) & 3;
                        break;
                    case 0:
                        if (battle_action_store_counter_ability(unit, 1, 0, 1) != 0) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        } else {
                            action[1] = 1;
                            action[0xA] = 5;
                        }
                        break;
                    case 1:
                        outcome = (battle_action_store_counter_ability(unit, 0xB, (s16)last_attack, 0) == 0) * 2;
                        break;
                    case 2:
                        if (battle_action_store_counter_ability(unit, 0xB, 0x93, 1) != 0) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        }
                        break;
                    case 3:
                        outcome = (battle_action_store_counter_ability(unit, 0x11,
                                       g_geomancy_terrain_ability_table
                                           [g_battle_map_tile_data[battle_map_calculate_location(unit)].surface.value
                                               & 0x3F],
                                       0)
                                      == 0)
                            * 2;
                        break;
                    case 7:
                        result = battle_action_select_auto_potion_item(unit);
                        if (result == -1) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                            break;
                        }
                        action[1] = 6;
                        action[8] = result;
                        action[0xA] = 5;
                        action[0xB] = i;
                        *(s16*)(action + 0xC) = unit->x;
                        *(s16*)(action + 0x10) = unit->position.bits.y;
                        *(u16*)(action + 0xE) = unit->position.raw >> 15;
                        result = battle_action_call_attack_preparation_at_preview(action);
                        outcome = BATTLE_REACTION_OUTCOME_COUNTER;
                        unit->last_ability_id = ABILITY_ID_REACTION_AUTO_POTION;
                        if (result == -1 || (u32)(result - 2) < 2) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        }
                        break;
                    case 8:
                        if (battle_action_store_counter_ability(unit, 1, 0, 1) != 0) {
                            outcome = BATTLE_REACTION_OUTCOME_NONE;
                        }
                        break;
                    default:
                        outcome = -(
                            (g_main_reaction_behavior_flags_by_ability_id[g_battle_current_reaction_ability_id] & 0x30)
                            == 0);
                        break;
                    }
                    if ((u32)(outcome - 1) < 2 && battle_action_check_and_consume_mp(unit) != 0) {
                        outcome = BATTLE_REACTION_OUTCOME_NONE;
                    }
                    if (outcome == BATTLE_REACTION_OUTCOME_NONE) {
                        main_util_copy_action_data(g_reaction_unit_action_data_16e, action);
                        i++;
                        continue;
                    }
                    if ((u32)(outcome - 1) < 3) {
                        battle_action_init_current_ability_strike_data(unit);
                    } else {
                        g_current_ability.strike_count = 1;
                        g_current_ability.strike_counter = 0;
                        g_current_ability.elemental_flags = 0;
                        g_current_ability.knockback_flags = 0;
                        g_current_ability.primary_weapon_id = unit->equipment[3];
                        g_current_ability.secondary_weapon_id = unit->equipment[5];
                        action[1] = 0;
                        action[0xA] = 5;
                        action[0xB] = i;
                        if (g_battle_current_reaction_ability_id == ABILITY_ID_REACTION_DAMAGE_SPLIT) {
                            action[0xB] = g_battle_acting_unit_id_byte;
                            unit = &g_battle_unit_stats[g_battle_acting_unit_id];
                        }
                        *(s16*)(action + 0xC) = unit->x;
                        *(s16*)(action + 0x10) = unit->position.bits.y;
                        *(u16*)(action + 0xE) = unit->position.raw >> 15;
                    }
                    *out_ability = *(u16*)&g_battle_current_reaction_ability_id;
                    action[0] = i;
                    return i;
                }
            }
        }
        i++;
    } while (i < 21);
    g_battle_action_context = 0;
    *out_ability = 0;
    return -1;
}
