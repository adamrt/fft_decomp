#include "fft/battle.h"

/*
 * Return the AI's usefulness category for one target.
 *
 * This is the acting side's heuristic, not the sign of HP damage or a success
 * guarantee. Nonzero check_reflect enables the timing-aware Reflect restriction.
 */
battle_ai_ability_effect_e battle_ai_classify_ability_effect(s32 unit_id, s32 check_reflect) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_stats_t* unit = &g_battle_unit_stats[unit_id];
    battle_ai_unit_decision_t* decision = &ai->unit_decisions[unit_id];
    s32 ct = g_battle_ai_data_base.considered_ability.ct;
    s32 i;
    s32 bit;
    s32 mask;
    s32 status;
    s32 result;
    s16 ability_id;

    if ((g_battle_ai_data_base.considered_ability.ai_flags.word & BATTLE_AI_ABILITY_ENEMIES_ONLY)
        && !decision->enemy_flag)
        return BATTLE_AI_ABILITY_EFFECT_NONE;
    if ((ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_ALLIES_ONLY) && decision->enemy_flag)
        return BATTLE_AI_ABILITY_EFFECT_NONE;
    if (unit_id == ai->acting_unit_id) {
        ability_id = ai->considered_ability.ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST
            && (g_main_ability_range_data[ability_id].flags_2 & ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER))
            return BATTLE_AI_ABILITY_EFFECT_NONE;
    }
    if (check_reflect && (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_REFLECTABLE)
        && battle_ai_is_status_active_through_delay(ct, unit, BATTLE_STATUS_ID_REFLECT))
        return BATTLE_AI_ABILITY_EFFECT_NONE;
    if ((ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_AFFECTED_BY_FAITH)
        && battle_ai_is_status_active_through_delay(ct, unit, BATTLE_STATUS_ID_INNOCENT))
        return BATTLE_AI_ABILITY_EFFECT_NONE;
    if (battle_ai_is_status_active_through_delay(ct, unit, BATTLE_STATUS_ID_JUMP))
        return BATTLE_AI_ABILITY_EFFECT_NONE;

    /* Death/revival timing determines which effects can still matter. */
    i = 0;
    /* BATTLE_STATUS_BYTE_MASK's u8 cast changes GCC 2.6.3 allocation in this
     * comma expression; retain the verified Dead mask literal here. */
    if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_CANCEL_STATUS)
        i = ((result = ai->considered_ability.parameters.bytes.status_infliction[0] & 0x20), result != 0);
    if (decision->targeting_flags_2 & BATTLE_AI_TARGET_DEAD_WITH_RERAISE) {
        s32 ability_ct = ai->considered_ability.ct;
        if (ability_ct < battle_ai_calculate_clockticks_until_unit_acts(unit))
            return BATTLE_AI_ABILITY_EFFECT_NONE;
        if (i)
            return BATTLE_AI_ABILITY_EFFECT_NONE;
    }
    if (decision->targeting_flags_2 & BATTLE_AI_TARGET_DEAD_WITHOUT_RERAISE) {
        s32 ability_ct = ai->considered_ability.ct;
        if (battle_ai_calculate_clockticks_until_death_counter_expires(unit) < ability_ct)
            return BATTLE_AI_ABILITY_EFFECT_NONE;
        if (!i)
            return BATTLE_AI_ABILITY_EFFECT_NONE;
    }
    if ((unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PETRIFY))
        && (!(ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_CANCEL_STATUS)
            || !(ai->considered_ability.parameters.bytes.status_infliction[1]
                & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PETRIFY))))
        return BATTLE_AI_ABILITY_EFFECT_NONE;
    if (battle_ai_is_status_active_through_delay(ct, unit, BATTLE_STATUS_ID_WALL)
        && (!(ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_CANCEL_STATUS)
            || !(ai->considered_ability.parameters.bytes.status_infliction[3]
                & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_WALL))))
        return BATTLE_AI_ABILITY_EFFECT_NONE;

    i = ai->considered_ability.element;
    if (i) {
        if (i == (i & unit->elemental_affinity[ELEMENTAL_AFFINITY_NULLIFY])
            || (i == 8 && (unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT)))) {
            return BATTLE_AI_ABILITY_EFFECT_NONE;
        }
    }

    if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_TARGET_ENEMIES) {
        if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_UNDEAD_REVERSE) {
            if (decision->enemy_flag) {
                if (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD))
                    return BATTLE_AI_ABILITY_EFFECT_ADVERSE;
            } else if (!(unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD))) {
                return BATTLE_AI_ABILITY_EFFECT_ADVERSE;
            }
        } else if (decision->enemy_flag) {
            if (unit->elemental_affinity[ELEMENTAL_AFFINITY_ABSORB] & ai->considered_ability.element)
                return BATTLE_AI_ABILITY_EFFECT_ADVERSE;
        } else {
            if (battle_ai_is_status_active_through_delay(ct, unit, BATTLE_STATUS_ID_CHARM)
                && battle_ai_check_unit_gets_turn_without_status(unit, BATTLE_STATUS_ID_CHARM))
                i = 1;
            else
                i = 0;
            if ((ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_AFFECTS_HP)
                && g_main_action_menu_types_by_skillset[ai->considered_ability.skillset] != ACTION_MENU_TYPE_CHARGE) {
                if (unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CONFUSION))
                    return BATTLE_AI_ABILITY_EFFECT_USEFUL;
                if (battle_ai_is_status_active_through_delay(ct, unit, BATTLE_STATUS_ID_SLEEP))
                    return BATTLE_AI_ABILITY_EFFECT_USEFUL;
                if (i)
                    return BATTLE_AI_ABILITY_EFFECT_USEFUL;
            }
            if ((unit->elemental_affinity[ELEMENTAL_AFFINITY_ABSORB] & ai->considered_ability.element)
                && (decision->targeting_flags_2 & BATTLE_AI_TARGET_HP_BELOW_HALF))
                return BATTLE_AI_ABILITY_EFFECT_USEFUL;
            if (!(unit->status_sets.current[1]
                    & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CONFUSION)
                        | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_BLOOD_SUCK)))
                && !i)
                return BATTLE_AI_ABILITY_EFFECT_ADVERSE;
            if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_ADD_STATUS) {
                /* Early acceptance of a disabling effect bypasses the generic
                 * already-present/immunity filters below. Status bytes 1..4
                 * are loaded together as an aligned word by the target. */
                s32 status_mask = ai->considered_ability.parameters.words.status_tail
                    & (BATTLE_AI_STATUS_TAIL_PACKED_MASK(BATTLE_STATUS_ID_SLOW)
                        | BATTLE_AI_STATUS_TAIL_PACKED_MASK(BATTLE_STATUS_ID_STOP)
                        | BATTLE_AI_STATUS_TAIL_PACKED_MASK(BATTLE_STATUS_ID_SLEEP)
                        | BATTLE_AI_STATUS_TAIL_PACKED_MASK(BATTLE_STATUS_ID_DONT_MOVE)
                        | BATTLE_AI_STATUS_TAIL_PACKED_MASK(BATTLE_STATUS_ID_DONT_ACT));
                if (status_mask)
                    return BATTLE_AI_ABILITY_EFFECT_USEFUL;
                /* Barrier: stops jump.c hoisting the USEFUL constant above the test and cross-jumping this return. */
                __asm__("");
                return BATTLE_AI_ABILITY_EFFECT_ADVERSE;
            }
            return BATTLE_AI_ABILITY_EFFECT_ADVERSE;
        }
        if ((ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_AFFECTS_HP)
            && !(unit->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEATH_SENTENCE)))
            return BATTLE_AI_ABILITY_EFFECT_USEFUL;
        if ((ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_AFFECTS_MP)
            && unit->mp > decision->lowest_mp_cost)
            return BATTLE_AI_ABILITY_EFFECT_USEFUL;
    }

    if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_TARGET_ALLIES) {
        if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_UNDEAD_REVERSE) {
            if (decision->enemy_flag) {
                result = BATTLE_AI_ABILITY_EFFECT_ADVERSE;
                if (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD))
                    return BATTLE_AI_ABILITY_EFFECT_USEFUL;
                return result;
            }
            if (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD))
                return BATTLE_AI_ABILITY_EFFECT_ADVERSE;
        } else if (decision->enemy_flag)
            return BATTLE_AI_ABILITY_EFFECT_ADVERSE;
        /* Death Sentence: the Add Reraise shortcut likewise bypasses the
         * generic already-present/immunity filters. */
        if (unit->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEATH_SENTENCE)) {
            /* Pin: flags in $v1 keep `result` in $v0; unpinned, `result` moves to $v1 function-wide. */

            register s32 ai_flags asm("$3") = ai->considered_ability.ai_flags.word;
            result = BATTLE_AI_ABILITY_EFFECT_NONE;
            if (ai_flags & BATTLE_AI_ABILITY_ADD_STATUS) {
                result = ai->considered_ability.parameters.bytes.status_infliction[2]
                    & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_RERAISE);
                /* Shared tail: returning here changes the target's branch layout. */
                goto return_nonzero;
            }
            goto return_value;
        }
        if ((ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_AFFECTS_HP)
            && (decision->targeting_flags_2 & BATTLE_AI_TARGET_HP_BELOW_HALF))
            return BATTLE_AI_ABILITY_EFFECT_USEFUL;
        if ((ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_AFFECTS_MP)
            && (decision->targeting_flags_2 & BATTLE_AI_TARGET_MP_CONSTRAINED))
            return BATTLE_AI_ABILITY_EFFECT_USEFUL;
    }

    if (!(ai->considered_ability.ai_flags.word & 3)) {
        if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_AFFECTS_HP)
            return BATTLE_AI_ABILITY_EFFECT_USEFUL;
        if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_AFFECTS_MP)
            return BATTLE_AI_ABILITY_EFFECT_USEFUL;
    }
    if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_ADD_STATUS) {
        for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
            mask = ai->considered_ability.parameters.bytes.status_infliction[i];
            mask ^= mask & unit->status_sets.current[i];
            if (mask) {
                mask ^= mask & unit->status_sets.immunity[i];
                if (mask) {
                    switch (i) {
                    case 1:
                        if ((mask & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SILENCE)) && !decision->silence_mod)
                            mask ^= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SILENCE);
                        if ((mask & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DARKNESS)) && decision->evade_mod < 2)
                            mask ^= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DARKNESS);
                        break;
                    case 2:
                        if ((mask & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_RERAISE))
                            && !(decision->targeting_flags_2 & BATTLE_AI_TARGET_HP_BELOW_HALF))
                            mask ^= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_RERAISE);
                        if ((mask & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG)) && !decision->enemy_flag)
                            mask ^= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG);
                        if (!(decision->targeting_flags_2 & BATTLE_AI_TARGET_HP_BELOW_THREE_QUARTERS))
                            mask &= (u8)~BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT);
                        break;
                    case 3:
                        if ((mask & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_REGEN))
                            && !(decision->targeting_flags_2 & BATTLE_AI_TARGET_HP_BELOW_HALF))
                            mask ^= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_REGEN);
                        if (!(decision->targeting_flags_2 & BATTLE_AI_TARGET_HP_BELOW_THREE_QUARTERS))
                            mask &= (u8) ~(BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PROTECT)
                                | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SHELL)
                                | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_WALL));
                        if ((mask
                                & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_HASTE)
                                    | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SLOW)))
                            && ((unit->status_sets.current[3] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_STOP))
                                || (unit->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SLEEP))))
                            mask &= (u8) ~(BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_HASTE)
                                | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SLOW));
                        break;
                    case 4:
                        if (!(decision->targeting_flags_2 & BATTLE_AI_TARGET_HP_BELOW_THREE_QUARTERS))
                            mask &= (u8) ~(BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FAITH)
                                | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_REFLECT));
                        break;
                    }
                    if (mask)
                        return BATTLE_AI_ABILITY_EFFECT_USEFUL;
                }
            }
        }
    }
    if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_CANCEL_STATUS) {
        for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
            mask = ai->considered_ability.parameters.bytes.status_infliction[i];
            mask &= unit->status_sets.current[i];
            if (mask) {
                mask ^= mask & unit->status_sets.innate[i];
                if (mask) {
                    if (i >= 3) {
                        for (bit = 0; bit < 8; bit++) {
                            s32 flag = 0x80 >> bit;
                            if (mask & flag) {
                                status = i * 8 + bit;
                                if (!battle_ai_is_status_active_through_delay(ct, unit, status)
                                    || !battle_ai_check_unit_gets_turn_without_status(unit, status))
                                    mask ^= flag;
                            }
                        }
                    }
                    switch (i) {
                    case 1:
                        if ((mask & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SILENCE)) && !decision->silence_mod)
                            mask ^= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SILENCE);
                        if ((mask & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DARKNESS)) && !decision->evade_mod)
                            mask ^= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DARKNESS);
                        break;
                    case 2:
                        if ((mask & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG)) && decision->enemy_flag)
                            mask ^= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG);
                        break;
                    }
                    if (mask)
                        return BATTLE_AI_ABILITY_EFFECT_USEFUL;
                }
            }
        }
    }
    /* Late stat-effect gate: Praise, Accumulate, and Yell only.
     * Odd parity still falls through to the Unequip flag below. */
    if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_AFFECTS_STATS) {
        ability_id = ai->considered_ability.ability_id;
        if (ability_id != ABILITY_ID_TALK_PRAISE && ability_id != ABILITY_ID_BASIC_SKILL_ACCUMULATE
            && ability_id != ABILITY_ID_BASIC_SKILL_YELL)
            return BATTLE_AI_ABILITY_EFFECT_USEFUL;
        if (!(rand() & 1))
            return BATTLE_AI_ABILITY_EFFECT_USEFUL;
    }
    result = ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_UNEQUIP;
return_nonzero:
    result = result != 0;
return_value:
    return result;
}
