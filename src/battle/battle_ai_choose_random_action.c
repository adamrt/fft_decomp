#include "fft/battle.h"
#include "fft/battle_ability.h"
#include "fft/battle_ai.h"
#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "fft/unit_slots.h"

/*
 * Choose a random action from the candidate movement and target masks.
 *
 * Math Skill first chooses learned ability and parameter combinations. Return
 * 1 after copying the current action, including fields unchanged by this pass,
 * or 0 when selection fails.
 */
s32 battle_ai_choose_random_action(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 value, skillset;
    u16* abilities;
    s32 index_or_count;

    g_battle_ai_data_base.current_action.target_flags_set = 0;
    g_battle_ai_data_base.current_action.skillset = ai->considered_ability.skillset;
    g_battle_ai_data_base.current_action.ability_id = ai->considered_ability.ability_id;
    g_battle_ai_data_base.current_action.item_id = ai->considered_ability.parameters.bytes.item_id;
    if (g_main_action_menu_types_by_skillset[ai->considered_ability.skillset] == ACTION_MENU_TYPE_ARITHMETICKS) {
        do {
            skillset = g_battle_math_source_skillsets[rand() % 4];
            abilities = main_ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ACTION);
            index_or_count = rand() % 16;
            value = abilities[index_or_count];
        } while (value == 0 || !(g_main_ability_data[value].ai_flags.bytes[3] & BATTLE_AI_ABILITY_FLAG_4_USABLE_BY_AI)
            || !(g_main_ability_range_data[value].flags_3 & ABILITY_SECONDARY_FLAG_3_CALCULATOR_ELIGIBLE)
            || !battle_ai_load_known_ability_flag(ai->acting_unit_id, skillset, index_or_count));
        ai->current_action.ability_id = value;
        do {
            value = rand() % 4;
        } while (!battle_ai_load_known_ability_flag(ai->acting_unit_id, ai->considered_ability.skillset, value));
        ai->current_action.calculator_type = value + ABILITY_ID_MATH_FIRST;
        do {
            value = rand() % 4 + 4;
        } while (!battle_ai_load_known_ability_flag(ai->acting_unit_id, ai->considered_ability.skillset, value));
        ai->current_action.calculator_multiplier = value + ABILITY_ID_MATH_FIRST;
    }
    if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_TARGET_MAP_TILES) {
        battle_ai_transfer_halfword_values(ai->targetable_tiles, (u16*)ai->reachable_tiles[0], 72);
        ai->movable_tile_count = battle_ai_count_flagged_tiles_within_map_bounds(ai->targetable_tiles);
        if (!ai->movable_tile_count)
            return 0;
        do {
            if (!battle_ai_take_random_tile(ai->targetable_tiles, &ai->current_action.coords, ai->movable_tile_count))
                return 0;
            ai->movable_tile_count--;
            if (battle_ai_check_ability_use_at_coords(&ai->current_action.coords)) {
                ai->current_action.wait_facing_hint = BATTLE_AI_DIRECTION_OVERLAP;
                goto selected;
            }
        } while (ai->movable_tile_count);
    } else {
        /* A single attempt block lets missing movement candidates exit early. */
        do {
            if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_FOLLOW_TARGET)
                ai->current_action.targeting_type = rand() % 2 + 5;
            else
                ai->current_action.targeting_type = 5;
            battle_ai_evaluate_linear_ability_behaviors();
            if (!ai->movable_tile_count) {
                if (ai->current_action.targeting_type == 6) {
                    ai->current_action.targeting_type = 5;
                    battle_ai_evaluate_linear_ability_behaviors();
                }
                if (!ai->movable_tile_count)
                    break;
            }
            do {
                if (!battle_ai_take_random_tile(ai->movable_tiles, &ai->current_action.coords, ai->movable_tile_count))
                    return 0;
                ai->movable_tile_count--;
                /* Keep the original count even when a rejected target bit is removed. */
                index_or_count = battle_ai_calculate_ability_range();
                if (index_or_count) {
                    do {
                        if (!battle_ai_take_random_tile(
                                ai->targetable_tiles, &ai->considered_unit_coords, index_or_count))
                            goto next_origin; /* continue the outer origin loop */
                        ai->current_action.target_x = ai->considered_unit_coords.bytes.x;
                        ai->current_action.target_y = ai->considered_unit_coords.bytes.y;
                        ai->current_action.target_elevation = ai->considered_unit_coords.bytes.elevation;
                        ai->current_action.wait_facing_hint = battle_ai_find_direction_of_target(
                            (s8*)&ai->considered_unit_coords, (s8*)&ai->current_action.coords);
                        if (ai->current_action.targeting_type != 6)
                            goto selected;
                    } while (
                        (ai->current_action.target_id = battle_ai_find_unit_at_coordinates(&ai->considered_unit_coords))
                        == BATTLE_UNIT_ID_NONE);
                    goto selected;
                }
            next_origin:;
            } while (ai->movable_tile_count);
        } while (0);
    }
    return 0;
selected: /* shared success tail */
    battle_ai_transfer_halfword_values(
        (u16*)&ai->selected_action, (u16*)&ai->current_action, sizeof(battle_ai_action_data_t));
    return 1;
}
