#include "fft/battle_ai.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/unit_slots.h"

/*
 * Initialize a unit's ability-use statistics and scenario targeting.
 *
 * Monster Skill-dependent entries count toward totals but not the MP,
 * Silence, or evasion numerators. Behavior updates depend on scenario flags.
 */
void battle_ai_init_unit_abilities(s32 unit_id) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_stats_t* unit = &g_battle_unit_stats[unit_id];
    battle_ai_unit_decision_t* decision;
    battle_ai_ability_entry_t* entry;
    s32 ability_count;
    s32 mp_count;
    s32 silence_count;
    s32 evade_count;
    s32 i;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE || ai->unit_battle_ids[unit_id] >= 16) {
        return;
    }
    battle_ai_build_unit_ability_list(unit_id);
    ability_count = 0;
    mp_count = 0;
    silence_count = 0;
    evade_count = 0;
    decision = &ai->unit_decisions[unit_id];
    decision->highest_mp_cost = 0;
    decision->lowest_mp_cost = 0xff;
    for (i = 0; i < 34; i++) {
        entry = &ai->ability_lists[ai->unit_battle_ids[unit_id]][i];
        if (entry->skillset_flags.bytes.skillset == SKILLSET_ID_NONE) {
            break;
        }
        battle_ai_load_ability_entry(entry);
        ability_count++;
        /* The target reads and updates skillset/usage as one halfword. */
        if (!(entry->skillset_flags.packed & (BATTLE_AI_ABILITY_ENTRY_REQUIRES_MONSTER_SKILL << 8))) {
            if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_AFFECTED_BY_SILENCE) {
                silence_count++;
            }
            if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_EVADEABLE) {
                evade_count++;
            }
            if (ai->considered_ability.mp_cost != 0) {
                mp_count++;
                if (decision->highest_mp_cost < ai->considered_ability.mp_cost) {
                    decision->highest_mp_cost = ai->considered_ability.mp_cost;
                }
                if (ai->considered_ability.mp_cost < decision->lowest_mp_cost) {
                    decision->lowest_mp_cost = ai->considered_ability.mp_cost;
                }
            }
        }
        entry->skillset_flags.bytes.usage_flags &= BATTLE_AI_ABILITY_ENTRY_INIT_PRESERVE_MASK;
        if (ai->considered_ability.ai_flags.word
            & (BATTLE_AI_ABILITY_DIRECT_TRAJECTORY | BATTLE_AI_ABILITY_ARC_TRAJECTORY)) {
            continue;
        }
        if (ai->considered_ability.ai_flags.bytes.flags_3 & BATTLE_AI_ABILITY_FLAG_3_EXHAUSTIVE_ORIGIN_MASK) {
            entry->skillset_flags.packed |= (BATTLE_AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN << 8);
        } else {
            if (ai->considered_ability.ability_id < ABILITY_ID_ITEM_FIRST) {
                if (ai->considered_ability.parameters.bytes.range + ai->considered_ability.parameters.bytes.aoe < 3
                    && (g_main_ability_range_data[ai->considered_ability.ability_id].flags_4
                        & ABILITY_SECONDARY_FLAG_4_EVADEABLE)) {
                    entry->skillset_flags.packed |= (BATTLE_AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN << 8);
                    continue;
                }
                if (g_main_ability_range_data[ai->considered_ability.ability_id].flags_2
                    & (ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER | ABILITY_SECONDARY_FLAG_2_CAN_TARGET_ENEMIES)) {
                    entry->skillset_flags.packed |= (BATTLE_AI_ABILITY_ENTRY_FORCE_CONSUME_EVALUATED_ORIGINS << 8);
                    continue;
                }
            }
            if (ai->considered_ability.parameters.bytes.aoe == 0) {
                entry->skillset_flags.packed |= (BATTLE_AI_ABILITY_ENTRY_FORCE_CONSUME_EVALUATED_ORIGINS << 8);
            }
        }
    }
    decision->silence_mod = battle_ai_calculate_ratio_times_4(ability_count, silence_count);
    decision->mp_ability_mod = battle_ai_calculate_ratio_times_4(ability_count, mp_count);
    decision->evade_mod = battle_ai_calculate_ratio_times_4(ability_count, evade_count);
    battle_ai_transfer_byte_values(&decision->flags, &unit->ai_flags, 5);
    decision->target.bytes.x = unit->ai_target_x;
    decision->target.bytes.y = unit->ai_target_y;
    decision->target.bytes.elevation = unit->ai_flags >> 7;
    decision->target.bytes.zero = 0;
    if (!(decision->flags & BATTLE_AI_DECISION_SPECIAL_BEHAVIOR)) {
        return;
    }
    if (decision->targeting_flags_2 & BATTLE_AI_TARGET_CONSERVE_CT) {
        ai->unit_behaviour[unit_id] = 0x11;
        return;
    }
    if (!(decision->flags & BATTLE_AI_DECISION_FOCUS_TARGET)) {
        return;
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (g_battle_unit_stats[i].entd_slot == BATTLE_ENTD_SLOT_NONE
            || g_battle_unit_stats[i].unit_id != decision->main_target_id) {
            continue;
        }
        decision->main_target_id = i;
        if (!(decision->flags & BATTLE_AI_DECISION_AGGRESSIVE)) {
            return;
        }
        unit->auto_battle_target = i;
        if ((g_battle_unit_stats[i].initial_team_flags & BATTLE_TEAM_MASK)
            != (unit->initial_team_flags & BATTLE_TEAM_MASK)) {
            ai->unit_behaviour[unit_id] = 0x0c;
        } else {
            ai->unit_behaviour[unit_id] = 0x0e;
        }
        return;
    }
    decision->flags &= ~BATTLE_AI_DECISION_SPECIAL_BEHAVIOR;
}
