#include "fft/battle.h"

/* Copied as ten byte-aligned bytes from 0x80193dd4; the target uses signed
 * byte loads for the final two bytes and unsigned loads while scanning. */
typedef struct battle_ai_math_ability_id_list {
    s8 ability_ids[10];
} battle_ai_math_ability_id_list_t;

extern battle_ai_math_ability_id_list_t g_battle_math_extra_known_bit_ids;
/* Borrowed from main's temporary skillset list, not a private AI copy. */

/*
 * Evaluate learned Math Skill ability and parameter combinations.
 *
 * Test four source skillsets and learned type/multiplier pairs. Persistent
 * indices resume a suspended priority check; -1 suspends and 0 ends this pass.
 */
s32 battle_ai_evaluate_math_targets(void) {
    battle_ai_math_ability_id_list_t special_abilities = g_battle_math_extra_known_bit_ids;
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    u8* special;
    s32 unit;

    if (g_battle_ai_data_base.decision_state)
        goto evaluate_priority; /* resume inside the multiplier loop */
    if (!battle_ai_check_ability_use_at_coords(&ai->acting_unit_coords))
        return 0;
    g_battle_ai_data_base.current_action.target_flags_set = 1;
    for (g_battle_ai_math_skillset_index = 0; g_battle_ai_math_skillset_index < 4; g_battle_ai_math_skillset_index++) {
        g_battle_ai_math_skillset = g_battle_math_source_skillsets[g_battle_ai_math_skillset_index];
        g_battle_ai_math_ability_list
            = main_ability_store_skillset_abilities(g_battle_ai_math_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        for (g_battle_ai_math_ability_index = 0; g_battle_ai_math_ability_index < 16;
            g_battle_ai_math_ability_index++) {
            /* Math targeting reads only the low byte of each halfword ID. */
            ai->math_ability_id = (u8)g_battle_ai_math_ability_list[g_battle_ai_math_ability_index];
            if (ai->math_ability_id == 0)
                continue;
            if (!(g_main_ability_range_data[ai->math_ability_id].flags_3
                    & ABILITY_SECONDARY_FLAG_3_CALCULATOR_ELIGIBLE))
                continue;
            if (!battle_ai_load_known_ability_flag(
                    ai->acting_unit_id, g_battle_ai_math_skillset, g_battle_ai_math_ability_index))
                continue;
            /* The extra known-bit call's result is ignored. The loop uses
             * the target's signed address comparison (SLT). */
            special = (u8*)special_abilities.ability_ids;
            do {
                if (ai->math_ability_id == *special++)
                    battle_ai_load_known_ability_flag(
                        ai->acting_unit_id, g_battle_ai_math_skillset, g_battle_ai_math_ability_index + 1);
            } while ((s32)special < (s32)&special_abilities.ability_ids[10]);
            battle_ai_transfer_halfword_values(
                (u16*)&ai->considered_ability.ai_flags, g_main_ability_data[ai->math_ability_id].ai_flags.halfwords, 4);
            battle_ai_transfer_byte_values(ai->considered_ability.parameters.bytes.status_infliction,
                g_main_status_infliction_data[g_main_ability_range_data[ai->math_ability_id].status_infliction_id]
                    .statuses,
                5);
            ai->considered_ability.ai_flags.bytes.flags_4 = BATTLE_AI_ABILITY_FLAG_4_USABLE_BY_AI;
            ai->current_action.ability_id = ai->math_ability_id;
            ai->considered_ability.element = g_main_ability_range_data[ai->math_ability_id].element;
            for (unit = 0; unit < BATTLE_UNIT_SLOT_COUNT; unit++)
                ai->targetability.live.unit_targetable[unit] = ai->targetability.live.unit_active[unit];
            if (!battle_ai_has_any_unit_decided_to_use_ability())
                continue;
            for (ai->math_type_index = 0; ai->math_type_index < 4; ai->math_type_index++) {
                if (!battle_ai_load_known_ability_flag(
                        ai->acting_unit_id, ai->considered_ability.skillset, ai->math_type_index))
                    continue;
                ai->math_multiplier_index = 4;
                ai->current_action.calculator_type = ai->math_type_index + ABILITY_ID_MATH_FIRST;
                do {
                    if (battle_ai_load_known_ability_flag(
                            ai->acting_unit_id, ai->considered_ability.skillset, ai->math_multiplier_index)) {
                        ai->current_action.calculator_multiplier = ai->math_multiplier_index + ABILITY_ID_MATH_FIRST;
                    evaluate_priority:
                        if (battle_ai_check_set_highest_unit_priority() == -1)
                            return -1;
                    }
                    ai->math_multiplier_index++;
                } while (ai->math_multiplier_index < 8);
            }
            battle_ai_insert_ranked_action();
        }
    }
    battle_ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    return 0;
}
