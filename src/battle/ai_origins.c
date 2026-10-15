/* Ai targets and coordinates. Its start is a cut chosen by topic; its end is a cut chosen by topic. */

#include "fft/battle_ai.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Copy byte_count / 2 16-bit values; an odd last byte is left alone. */
void battle_mem_copy_u16_values(void* destination, const void* source, s32 byte_count) {
    u16* dst = destination;
    const u16* src = source;
    s32 count = byte_count / 2;
    s32 i;
    for (i = 0; i < count; i++) {
        *dst++ = *src++;
    }
}

void battle_mem_copy_bytes(void* destination, const void* source, s32 len) {
    u8* dst = destination;
    const u8* src = source;
    s32 end;
    if (len > 0) {
        end = len + (s32)dst;
        do {
            *dst = *src;
            dst = dst + 1;
            src = src + 1;
        } while ((s32)dst < end);
    }
}

/* Zero byte_count / 4 32-bit values starting at dst. */
void battle_mem_zero_32bit_values(void* destination, s32 byte_count) {
    s32* dst = destination;
    s32 i;

    byte_count /= 4;
    for (i = 0; i < byte_count; i++) {
        *dst++ = 0;
    }
}

typedef union ai_direction_viability {
    u32 word;
    u8 by_direction[AI_CARDINAL_DIRECTION_COUNT];
} ai_direction_viability_t;

/* Separate locals change the stack slots and coordinate byte stores. */
typedef struct ai_wait_facing_work {
    ai_coords_t target_coords;
    u8 _unused_04[4];
    ai_direction_viability_t viability;
    /* Read as viability.by_direction[AI_DIRECTION_OVERLAP] and never
     * written, so it holds a leftover stack byte (QUIRKS.md). */
    u8 past_viable_directions;
} ai_wait_facing_work_t;

/*
 * Choose a facing hint for an AI-controlled unit's Wait command.
 *
 * Values other than 5 are returned directly. Otherwise prefer the nearest
 * imminent enemy, then any enemy, while rejecting blocked or steeply rising
 * adjacent tiles. The original does not bounds-check the direction lookup, so
 * coincident source and target coordinates can read one byte past the four-byte
 * viability array.
 */
s32 battle_ai_choose_wait_facing(void) {
    ai_data_t* ai = &g_ai_data_base;
    ai_wait_facing_work_t work;
    s32 direction;
    s32 target_id;
    s32 acting_unit_id;
    s32 fallback_direction;
    s32 tried_any_enemy;
    s32 facing;

    if (ai->selected_action.wait_facing_hint != AI_DIRECTION_CHOOSE_AT_WAIT)
        return ai->selected_action.wait_facing_hint;

    battle_ai_copy_unit_coordinates(ai->acting_unit_id, &ai->acting_unit_coords);
    for (direction = 0; direction < AI_CARDINAL_DIRECTION_COUNT; direction++) {
        work.target_coords.fields.x
            = ai->acting_unit_coords.fields.x + g_ai_facing_tile_offsets.by_direction[direction][0];
        work.target_coords.fields.y
            = ai->acting_unit_coords.fields.y + g_ai_facing_tile_offsets.by_direction[direction][1];
        work.target_coords.fields.elevation = 0;
        if (battle_ai_is_facing_tile_blocked(&ai->acting_unit_coords, &work.target_coords) != 0) {
            work.viability.by_direction[direction] = 0;
        } else {
            fallback_direction = direction;
            work.viability.by_direction[direction] = 1;
        }
    }
    if (work.viability.word == 0)
        work.viability.word = 0x01010101;

    for (target_id = 0; target_id < BATTLE_UNIT_SLOT_COUNT; target_id++) {
        ai->targetability.live.unit_targetable[target_id] = 1;
    }
    tried_any_enemy = 0;
    for (;;) {
        acting_unit_id = ai->acting_unit_id;
        target_id = battle_ai_find_nearest_target(AI_NEAREST_IMMINENT_ENEMY);
        if (acting_unit_id == target_id) {
            target_id = battle_ai_find_nearest_target(AI_NEAREST_ENEMY);
            tried_any_enemy = 1;
        }
        battle_ai_copy_unit_coordinates(target_id, &work.target_coords);
        direction = battle_ai_find_direction_of_target(&work.target_coords, &ai->acting_unit_coords);
        if (work.viability.by_direction[direction] != 0)
            return direction;
        if (tried_any_enemy == 0) {
            ai->targetability.live.unit_targetable[target_id] = 0;
            continue;
        }
        facing = ai->acting_unit->position.facing;
        if (work.viability.by_direction[facing] == 0)
            return fallback_direction;
        return facing;
    }
}

/*
 * Reject a facing candidate outside the map or too far above the source.
 *
 * Return 1 for rejection and 0 otherwise. Tile flag 0 also rejects a candidate.
 * Only the destination's lower layer is checked; a rise of 3 is rejected,
 * but descents are unrestricted. Slope and depth do not affect this test.
 */
s32 battle_ai_is_facing_tile_blocked(const ai_coords_t* source, const ai_coords_t* target) {
    u8 x = target->fields.x;
    u8 width = g_ai_data_base.map_max_x;
    u8 y;
    map_tile_t* tile;

    if (x >= width) {
        return 1;
    }
    y = target->fields.y;
    if (y >= g_ai_data_base.map_max_y || (tile = &g_map_tile_data[y * width + x])->flags.blocked) {
        return 1;
    }
    return tile->height
        - g_map_tile_data[(source->fields.elevation * MAP_TILES_PER_LAYER) + source->fields.y * width
            + source->fields.x]
              .height
        >= 3;
}

/* Target 0x8019ae30. */
void battle_ai_init_selected_action(void) {
    battle_ai_reset_action_ranking_fields(&g_ai_data_base.selected_action);
}

/*
 * Remember a triggered trap unless its tile is already in the cache.
 *
 * Current coordinates and selected-action defaults are updated even when the
 * list is full. Equality includes all four bytes; x == 0xff marks an empty slot.
 */
void battle_ai_record_triggered_trap_coords(s32 x, s32 y, s32 level) {
    ai_data_t* ai = &g_ai_data_base;
    s32 i;

    g_ai_data_base.considered_unit_coords.fields.x = x;
    g_ai_data_base.considered_unit_coords.fields.y = y;
    g_ai_data_base.considered_unit_coords.fields.elevation = level;
    g_ai_data_base.considered_unit_coords.fields.zero = 0;
    battle_ai_init_selected_action();
    for (i = 0; i < AI_KNOWN_TRAP_COUNT; i++) {
        if (ai->known_trap_coords[i].fields.x == 0xff) {
            ai->known_trap_coords[i] = ai->considered_unit_coords;
            break;
        }
        if (ai->known_trap_coords[i].word == ai->considered_unit_coords.word) {
            break;
        }
    }
}

/* 1 once 325 scanlines have passed since the last vertical blank: the
 * pathfinding loops use it to yield for the frame. */
s32 battle_ai_is_pathfinding_budget_exhausted(void) {
    return VSync(1) >= AI_PATHFIND_SCANLINE_LIMIT;
}

/*
 * Evaluate target alternatives for the considered ability.
 *
 * Return -1 to suspend and 0 when finished, including rejected abilities.
 * Suspended target searches resume without reinitializing the candidate.
 */
s32 battle_ai_evaluate_ability_outcome(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 i;
    if (g_ai_data_base.resume_pending) {
        /* Resume: each state re-enters the evaluation step that suspended. */
        switch (g_ai_data_base.outcome_evaluation_state) {
        case AI_ABILITY_EVALUATION_REFLECTED_TARGETS:
            goto evaluate_reflected_targets;
        case AI_ABILITY_EVALUATION_SELF_TARGET_RANGE:
            goto evaluate_self_target_range;
        case AI_ABILITY_EVALUATION_UNIT_TARGETS:
            goto evaluate_single_unit_pass;
        case AI_ABILITY_EVALUATION_FIRST_CT_GROUP:
            goto evaluate_first_ct_group;
        case AI_ABILITY_EVALUATION_SECOND_CT_GROUP:
            goto evaluate_second_ct_group;
        case AI_ABILITY_EVALUATION_MATH_TARGETS:
            goto evaluate_math_targets;
        case AI_ABILITY_EVALUATION_PREPARE:
            break;
        case AI_ABILITY_EVALUATION_MAP_TARGETS:
            goto evaluate_map_targets;
        default:
            goto evaluate_tile_targets;
        }
    }
    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_PREPARE;
        return -1;
    }
    {
        s32 flags = ai->considered_ability.ai_flags.word;
        ai->resume_pending = 0;
        if (flags >= 0)
            return 0;
        if (flags & AI_ABILITY_RANDOM_USE) {
            i = ai->random_use_threshold;
            if (i < rand() % AI_ABILITY_SHARE_SCALE)
                return 0;
        }
    }
    ai->current_action.command.skillset_id = ai->considered_ability.skillset;
    ai->current_action.command.ability_id = ai->considered_ability.ability_id;
    /* Item selection updates only the low byte of the 16-bit command field. */
    *(u8*)&ai->current_action.command.item_or_equipment = ai->considered_ability.parameters.fields.item_id;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++)
        ai->targetability.live.unit_targetable[i] = ai->targetability.live.unit_active[i];
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_TARGET_MAP_TILES) {
        s32 acting_id = ai->acting_unit_id;
        ai->current_action.command.targeting_type = BATTLE_TARGETING_TYPE_UNIT;
        ai->current_action.command.target_id = acting_id;
        ai->current_action.coords = ai->candidate_coords[ai->movement_scenario];
        battle_ai_update_tile_pickup();
        if (g_main_action_menu_types_by_skillset[ai->considered_ability.skillset] == ACTION_MENU_TYPE_ARITHMETICKS) {
        evaluate_math_targets:
            if (battle_ai_evaluate_math_targets() == -1) {
                ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_MATH_TARGETS;
                return -1;
            }
            return 0;
        }
        if (!battle_ai_filter_useful_targets())
            return 0;
    evaluate_map_targets:
        if (battle_ai_evaluate_cast_origins() == -1) {
            ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_MAP_TARGETS;
            return -1;
        }
        battle_ai_insert_ranked_action();
        return 0;
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_REFLECTABLE) {
    evaluate_reflected_targets:
        if (battle_ai_evaluate_reflected_target_origins() == -1) {
            ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_REFLECTED_TARGETS;
            return -1;
        }
    }
    if (!battle_ai_filter_useful_targets())
        return 0;
    if (ai->targetability.live.unit_targetable[ai->acting_unit_id]) {
    evaluate_self_target_range:
        if (battle_ai_evaluate_self_target_origins() == -1) {
            ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_SELF_TARGET_RANGE;
            return -1;
        }
    }
    ai->ability_effect_on_self = battle_ai_classify_ability_effect(ai->acting_unit_id, 1);
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_FOLLOW_TARGET) {
        ai->current_action.command.targeting_type = BATTLE_TARGETING_TYPE_UNIT;
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_CONSUME_EVALUATED_ORIGINS) {
            ai->targetability.live.target_setting_flags |= AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS;
        evaluate_single_unit_pass:
            if (battle_ai_evaluate_movement_origins() == -1) {
                ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_UNIT_TARGETS;
                return -1;
            }
        } else {
            /* Evaluate complementary CT-filtered target groups, then restore the
             * original eligibility list. The overlapping flag byte stays intact. */
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                ai->targetability.snapshot.unit_targetable_saved[i] = ai->targetability.live.unit_targetable[i];
                if (ai->targetability.live.unit_targetable[i] && !battle_ai_can_unit_act_within_charge_time(i))
                    ai->targetability.live.unit_targetable[i] = 0;
            }
            ai->targetability.live.target_setting_flags |= AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS;
        evaluate_first_ct_group:
            if (battle_ai_evaluate_movement_origins() == -1) {
                ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_FIRST_CT_GROUP;
                return -1;
            }
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++)
                if (ai->targetability.snapshot.unit_targetable_saved[i]) {
                    if (battle_ai_can_unit_act_within_charge_time(i))
                        ai->targetability.live.unit_targetable[i] = 0;
                    else
                        ai->targetability.live.unit_targetable[i] = 1;
                }
            ai->targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS;
        evaluate_second_ct_group:
            if (battle_ai_evaluate_movement_origins() == -1) {
                ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_SECOND_CT_GROUP;
                return -1;
            }
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++)
                ai->targetability.live.unit_targetable[i] = ai->targetability.snapshot.unit_targetable_saved[i];
        }
        if (!ai->considered_ability.parameters.fields.aoe) {
            battle_ai_insert_ranked_action();
            return 0;
        }
    }
    if (battle_ai_exclude_enemies_acting_before_ct()) {
        ai->current_action.command.targeting_type = BATTLE_TARGETING_TYPE_TILE;
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_CONSUME_EVALUATED_ORIGINS)
            ai->targetability.live.target_setting_flags |= AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS;
    evaluate_tile_targets:
        if (battle_ai_evaluate_movement_origins() == -1) {
            ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_TILE_TARGETS;
            return -1;
        }
    }
    battle_ai_insert_ranked_action();
    return 0;
}

/*
 * Evaluate self-target alternatives from the actor and candidate coordinates.
 *
 * Record the resume stage when returning -1 to suspend. Completed searches
 * restore the actor coordinates and submit the action to the ranked list.
 */
s32 battle_ai_evaluate_self_target_origins(void) {
    ai_data_t* ai = &g_ai_data_base;
    ai_tile_mask_mode_e mode;

    if (g_ai_data_base.resume_pending) {
        /* Resume: re-enter the step that suspended. */
        if (g_ai_self_target_origin_stage == 0)
            goto evaluate_map;
        goto evaluate_targets;
    }
    g_ai_data_base.targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS;
    g_ai_data_base.current_action.coords = ai->candidate_coords[g_ai_data_base.movement_scenario];
    battle_ai_update_tile_pickup();
    if ((g_ai_data_base.considered_ability.ai_flags.word & AI_ABILITY_FOLLOW_TARGET)
        && !(g_main_ability_range_data[g_ai_data_base.considered_ability.ability_id].flags_1
            & ABILITY_SECONDARY_FLAG_1_CANNOT_TARGET_SELF)) {
        g_ai_data_base.current_action.act_before_move = 1;
        g_ai_data_base.current_action.command.targeting_type = BATTLE_TARGETING_TYPE_UNIT;
        g_ai_data_base.current_action.command.target_id = g_ai_data_base.acting_unit_id;
    evaluate_map:
        if (battle_ai_evaluate_cast_origins() == -1) {
            g_ai_self_target_origin_stage = 0;
            return -1;
        }
    }
    ai->current_action.command.targeting_type = BATTLE_TARGETING_TYPE_TILE;
    g_ai_self_target_origin_stage = 0;
    ai->current_action.act_before_move = 1;
    /* The 32-bit view of the first member keeps this load after the phase store.
     * Direct .word or whole-union assignment changes target scheduling. */
    ai->considered_unit_coords.word = *(u32*)&ai->acting_unit_coords;
prepare_origin:
    mode = AI_TILE_MASK_AOE;
    if (ai->current_action.command.targeting_type == BATTLE_TARGETING_TYPE_UNIT)
        mode = AI_TILE_MASK_EXPLICIT;
    battle_ai_build_targetable_tile_mask(mode, 0);
    battle_mem_copy_u16_values(ai->attack_origin_tiles, ai->targetable_tiles, sizeof(ai->attack_origin_tiles));
    if (battle_ai_is_ability_origin_allowed(&ai->acting_unit_coords)) {
    evaluate_targets:
        if (battle_ai_evaluate_target_tiles() == -1) {
            /* A first-origin suspension skips the moved-origin pass on resume. */
            g_ai_self_target_origin_stage = 1;
            return -1;
        }
    }
    if (g_ai_self_target_origin_stage == 0) {
        ai->current_action.act_before_move = 0;
        ai->considered_unit_coords = ai->current_action.coords;
        g_ai_self_target_origin_stage++;
        /* A goto retry, not a loop: GCC's loop pass would move the suspend
         * exit out of line. */
        goto prepare_origin;
    }
    battle_ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    battle_ai_insert_ranked_action();
    return 0;
}

/*
 * Check depth, terrain, and Monster Skill restrictions at an AI origin.
 *
 * Return 1 when these checks permit use, otherwise 0. Geomancy delegates
 * directly to its terrain check; this is not a complete ability-use test.
 */
s32 battle_ai_is_ability_origin_allowed(const ai_coords_t* coords) {
    ai_data_t* ai = &g_ai_data_base;
    s32 i;
    s32 dx;
    s32 dy;
    s32 height;
    unit_stats_t* unit;

    if (g_ai_data_base.water_penalty) {
        s32 depth = g_map_tile_data[coords->fields.y * g_ai_data_base.map_max_x + coords->fields.x
            + (coords->fields.elevation * MAP_TILES_PER_LAYER)]
                        .depth_half_height.depth;
        if (depth >= 2)
            return 0;
    }
    if (g_main_action_menu_types_by_skillset[ai->considered_ability.skillset] == ACTION_MENU_TYPE_ELEMENTS)
        return battle_ai_can_use_geomancy(coords);
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_REQUIRES_MONSTER_SKILL) {
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            unit = &g_battle_unit_stats[i];
            if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
                continue;
            if (!(unit->ability_flags.support[3] & BATTLE_SUPPORT_SET_4_MONSTER_SKILL))
                continue;
            if ((unit->initial_team_flags & BATTLE_TEAM_MASK) != ai->acting_unit_team)
                continue;
            if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_PREVENT_REACTION))
                continue;
            dx = unit->x - coords->fields.x;
            if (dx < 0)
                dx = -dx;
            dy = unit->position.y - coords->fields.y;
            if (dy < 0)
                dy = -dy;
            if (dx + dy < 2) {
                height = battle_ai_calculate_height_difference_between_units(coords, unit);
                if (height >= -2 && height <= 2)
                    return 1;
            }
        }
        return 0;
    }
    return 1;
}

/*
 * Compare the unit tile's height with the supplied AI coordinates.
 *
 * Return their signed half-height difference divided by 2, truncating
 * toward 0. unit body height and water depth are not included.
 */
s32 battle_ai_calculate_height_difference_between_units(const ai_coords_t* coords, const unit_stats_t* unit) {
    map_tile_t* tile;
    s32 h1;
    s32 h2;

    tile = &g_map_tile_data[unit->position.higher_elevation * MAP_TILES_PER_LAYER
        + unit->position.y * g_ai_data_base.map_max_x + unit->x];
    h1 = tile->height * 2 + tile->depth_half_height.half_height;
    tile = &g_map_tile_data[(coords->fields.elevation * MAP_TILES_PER_LAYER)
        + coords->fields.y * g_ai_data_base.map_max_x + coords->fields.x];
    h2 = tile->height * 2 + tile->depth_half_height.half_height;
    return (h1 - h2) / 2;
}

/*
 * Test whether the terrain selects the considered Geomancy ability.
 *
 * Return 1 for the terrain table's matching ability ID, otherwise 0.
 */
s32 battle_ai_can_use_geomancy(const ai_coords_t* coords) {
    u32 tile_id;
    u32 tile_type;
    u32 ability_terrain;

    tile_id = (coords->fields.elevation * MAP_TILES_PER_LAYER) + coords->fields.y * g_ai_data_base.map_max_x
        + coords->fields.x;
    tile_type = g_map_tile_data[tile_id].surface.type;
    ability_terrain = g_main_terrain_geomancy_abilities[tile_type];
    return ability_terrain == g_ai_data_base.considered_ability.ability_id;
}

/*
 * Evaluate reachable origins for reflecting an ability onto another unit.
 *
 * Return -1 to suspend and 0 after restoring the temporary unit's position
 * and submitting the action to the ranked list. Stored counters survive
 * suspension, but the target does not reload the local reflector pointer
 * on resume; later target iterations can therefore use the caller's s2.
 */
s32 battle_ai_evaluate_reflected_target_origins(void) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* reflector;
    ai_unit_decision_t* decision;
    ai_tile_mask_mode_e mode;
    s32 ct;

    if (g_ai_data_base.resume_pending) {
        /* Resume: the target jumps into the middle of the origin loop. */
        goto evaluate_targets;
    }
    g_ai_data_base.current_action.reflected_action = 1;
    g_ai_data_base.targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS;
    for (ai->reflector_unit_index = 0; ai->reflector_unit_index < BATTLE_UNIT_SLOT_COUNT; ai->reflector_unit_index++) {
        reflector = &g_battle_unit_stats[ai->reflector_unit_index];
        decision = &ai->unit_decisions[ai->reflector_unit_index];
        if (decision->targeting_flags_2 & AI_TARGET_UNTARGETABLE)
            continue;
        if (ai->reflector_unit_index == ai->acting_unit_id)
            continue;
        if (!battle_ai_is_status_active_through_delay(ai->considered_ability.ct, reflector, STATUS_ID_REFLECT))
            continue;
        if (battle_ai_can_unit_act_within_charge_time(ai->reflector_unit_index))
            continue;
        if (decision->targeting_flags_2 & AI_TARGET_DEAD_WITHOUT_RERAISE) {
            ct = ai->considered_ability.ct;
            if (battle_ai_calculate_clockticks_until_death_counter_expires(reflector) < ct)
                continue;
        }
        ai->current_action.command.targeting_type = BATTLE_TARGETING_TYPE_TILE;
        battle_ai_copy_unit_coordinates(ai->reflector_unit_index, &ai->considered_unit_coords);
        mode = AI_TILE_MASK_AOE;
        if (ai->current_action.command.targeting_type == BATTLE_TARGETING_TYPE_UNIT)
            mode = AI_TILE_MASK_EXPLICIT;
        battle_ai_build_targetable_tile_mask(mode, 0);
        battle_mem_copy_u16_values(ai->attack_origin_tiles, ai->targetable_tiles, sizeof(ai->attack_origin_tiles));
        for (ai->reflected_target_unit_index = 0; ai->reflected_target_unit_index < BATTLE_UNIT_SLOT_COUNT;
            ai->reflected_target_unit_index++) {
            if (!ai->targetability.live.unit_targetable[ai->reflected_target_unit_index])
                continue;
            if (ai->reflected_target_unit_index == ai->reflector_unit_index)
                continue;
            if (ai->reflected_target_unit_index == ai->acting_unit_id)
                continue;
            if (battle_ai_can_unit_act_within_charge_time(ai->reflected_target_unit_index))
                continue;
            if (battle_ai_classify_ability_effect(ai->reflected_target_unit_index, 0) != AI_ABILITY_EFFECT_USEFUL)
                continue;
            ai->current_action.coords.fields.x
                = reflector->x * 2 - g_battle_unit_stats[ai->reflected_target_unit_index].x;
            ai->current_action.coords.fields.y
                = reflector->position.y * 2 - g_battle_unit_stats[ai->reflected_target_unit_index].position.y;
            if (ai->current_action.coords.fields.x >= ai->map_max_x)
                continue;
            if (ai->current_action.coords.fields.y >= ai->map_max_y)
                continue;
            for (ai->reflected_candidate_level = 0; ai->reflected_candidate_level < MAP_TERRAIN_LAYER_COUNT;
                ai->reflected_candidate_level++) {
                ai->current_action.coords.fields.elevation = ai->reflected_candidate_level;
                if ((ai->reachable_tiles[ai->movement_scenario][ai->reflected_candidate_level]
                                        [ai->current_action.coords.fields.y]
                        << ai->current_action.coords.fields.x)
                    & 0x8000) {
                    battle_ai_update_tile_pickup();
                    ai->current_action.act_before_move = 1;
                    ai->reflected_origin_phase = 0;
                    for (;;) {
                        if (battle_ai_is_ability_origin_allowed(&ai->acting_unit_coords)) {
                        evaluate_targets:
                            if (battle_ai_evaluate_target_tiles() == -1)
                                return -1;
                        }
                        if (ai->reflected_origin_phase != 0)
                            break;
                        ai->reflected_origin_phase = 1;
                        ai->current_action.act_before_move = 0;
                    }
                }
            }
        }
    }
    battle_ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    ai->current_action.reflected_action = 0;
    battle_ai_insert_ranked_action();
    return 0;
}

/* Classify the crystal or treasure occupant of the current action tile. */
void battle_ai_update_tile_pickup(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 unit_id;
    unit_stats_t* unit;

    g_ai_data_base.tile_pickup_kind = AI_TILE_PICKUP_NONE;
    unit_id = battle_ai_find_unit_at_coordinates(&ai->current_action.coords);
    if (unit_id != BATTLE_UNIT_ID_NONE) {
        unit = &g_battle_unit_stats[unit_id];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL)) {
                g_ai_data_base.tile_pickup_kind = AI_TILE_PICKUP_CRYSTAL;
            }
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
                g_ai_data_base.tile_pickup_kind = AI_TILE_PICKUP_TREASURE;
            }
        }
    }
}

/*
 * Evaluate learned Math Skill ability and parameter combinations.
 *
 * Test four source skillsets and learned type/multiplier pairs. Persistent
 * indices resume a suspended priority check; -1 suspends and 0 ends this pass.
 */
s32 battle_ai_evaluate_math_targets(void) {
    u8 special_abilities[AI_MATH_EXTRA_KNOWN_ABILITY_COUNT];
    ai_data_t* ai = &g_ai_data_base;
    s32 i;
    s32 unit;

    memcpy(special_abilities, g_ai_math_extra_known_ability_ids, sizeof(special_abilities));
    if (g_ai_data_base.resume_pending)
        goto evaluate_priority; /* resume inside the multiplier loop */
    if (!battle_ai_is_ability_origin_allowed(&ai->acting_unit_coords))
        return 0;
    g_ai_data_base.current_action.act_before_move = 1;
    for (g_ai_math_skillset_index = 0; g_ai_math_skillset_index < AI_MATH_SOURCE_SKILLSET_COUNT;
        g_ai_math_skillset_index++) {
        g_ai_math_skillset_id = g_ai_math_source_skillsets[g_ai_math_skillset_index];
        g_ai_math_ability_list
            = main_ability_store_skillset_abilities(g_ai_math_skillset_id, SKILLSET_ABILITY_FILTER_ACTION);
        for (g_ai_math_ability_index = 0; g_ai_math_ability_index < HUMAN_SKILLSET_ACTION_SLOT_COUNT;
            g_ai_math_ability_index++) {
            /* Math targeting reads only the low byte of each 16-bit ID. */
            ai->math_ability_id = g_ai_math_ability_list[g_ai_math_ability_index];
            if (ai->math_ability_id == 0)
                continue;
            if (!(g_main_ability_range_data[ai->math_ability_id].flags_3
                    & ABILITY_SECONDARY_FLAG_3_CALCULATOR_ELIGIBLE))
                continue;
            if (!battle_ai_is_ability_known(ai->acting_unit_id, g_ai_math_skillset_id, g_ai_math_ability_index))
                continue;
            /* The extra known-bit call's result is ignored. */
            for (i = 0; i < AI_MATH_EXTRA_KNOWN_ABILITY_COUNT; i++) {
                if (ai->math_ability_id == special_abilities[i])
                    battle_ai_is_ability_known(ai->acting_unit_id, g_ai_math_skillset_id, g_ai_math_ability_index + 1);
            }
            battle_mem_copy_u16_values(
                &ai->considered_ability.ai_flags, &g_main_ability_data[ai->math_ability_id].ai_flags, 4);
            battle_mem_copy_bytes(ai->considered_ability.parameters.fields.status_infliction,
                g_main_status_infliction_data[g_main_ability_range_data[ai->math_ability_id].status_infliction_id]
                    .statuses,
                5);
            ai->considered_ability.ai_flags.fields.flags_4 = AI_ABILITY_FLAG_4_USABLE_BY_AI;
            ai->current_action.command.ability_id = ai->math_ability_id;
            ai->considered_ability.element = g_main_ability_range_data[ai->math_ability_id].element;
            for (unit = 0; unit < BATTLE_UNIT_SLOT_COUNT; unit++)
                ai->targetability.live.unit_targetable[unit] = ai->targetability.live.unit_active[unit];
            if (!battle_ai_filter_useful_targets())
                continue;
            for (ai->math_type_index = 0; ai->math_type_index < CALCULATOR_ATTRIBUTE_ABILITY_COUNT;
                ai->math_type_index++) {
                if (!battle_ai_is_ability_known(
                        ai->acting_unit_id, ai->considered_ability.skillset, ai->math_type_index))
                    continue;
                ai->math_multiplier_index = CALCULATOR_ATTRIBUTE_ABILITY_COUNT;
                ai->current_action.command.calculator_type = ai->math_type_index + ABILITY_ID_MATH_FIRST;
                do {
                    if (battle_ai_is_ability_known(
                            ai->acting_unit_id, ai->considered_ability.skillset, ai->math_multiplier_index)) {
                        ai->current_action.command.calculator_multiplier
                            = ai->math_multiplier_index + ABILITY_ID_MATH_FIRST;
                    evaluate_priority:
                        if (battle_ai_evaluate_current_action() == -1)
                            return -1;
                    }
                    ai->math_multiplier_index++;
                } while (ai->math_multiplier_index < CALCULATOR_ABILITY_COUNT);
            }
            battle_ai_insert_ranked_action();
        }
    }
    battle_ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    return 0;
}

/*
 * Evaluate target tiles from eligible movement origins.
 *
 * Return -1 with the search stage saved when time or a child suspends;
 * return 0 after restoring the temporary unit's position. The scan path
 * reloads its full row mask on resume and continues at its saved x index.
 */
s32 battle_ai_evaluate_movement_origins(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 row;

    if (g_ai_data_base.resume_pending) {
        /* Resume: re-enter the step that suspended. */
        if (g_ai_data_base.highest_priority_state == AI_ORIGIN_EVALUATION_PREPARE)
            goto begin_evaluation;
        if (g_ai_data_base.highest_priority_state == AI_ORIGIN_EVALUATION_RANKED)
            goto evaluate_ranked_origin;
        row = ai->movable_tiles[g_ai_data_base.map_level_counter][g_ai_data_base.y_counter];
        goto evaluate_scanned_origin;
    }
    row = ai->targetability.live.unit_targetable[g_ai_data_base.acting_unit_id];
    ai->targetability.live.unit_targetable[g_ai_data_base.acting_unit_id] = 0;
    battle_ai_build_attack_origin_masks();
    ai->targetability.live.unit_targetable[g_ai_data_base.acting_unit_id] = row;
begin_evaluation:
    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        ai->highest_priority_state = AI_ORIGIN_EVALUATION_PREPARE;
        return -1;
    }
    ai->resume_pending = 0;
    ai->ranked_origin_scan = 0;
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_EXHAUSTIVE_ORIGIN_SCAN) {
        ai->targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS;
    } else {
        ai->max_possibilities = ai->attack_origin_tile_count;
        ai->max_possibilities *= ai->movable_tile_count;
        if (ai->max_possibilities > 10)
            ai->ranked_origin_scan = 1;
        /* The target sets this regardless of the truncated product test. */
        ai->ranked_origin_scan = 1;
    }
    if ((ai->targetability.live.target_setting_flags & AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS)
        || ai->ranked_origin_scan) {
        while (ai->attack_origin_tile_count && ai->movable_tile_count) {
            battle_ai_take_next_movement_origin();
            battle_ai_finalize_action_destination();
        evaluate_ranked_origin:
            row = battle_ai_evaluate_target_tiles();
            if (row == -1) {
                ai->highest_priority_state = AI_ORIGIN_EVALUATION_RANKED;
                return -1;
            }
        }
    } else {
        ai->map_level_counter = 0;
        do {
            for (ai->y_counter = 0; ai->y_counter < ai->map_max_y; ai->y_counter++) {
                row = ai->movable_tiles[ai->map_level_counter][ai->y_counter];
                if (row != 0) {
                    for (ai->max_possibilities = 0; ai->max_possibilities < ai->map_max_x; ai->max_possibilities++) {
                        if ((row << ai->max_possibilities) & 0x8000) {
                            ai->current_action.coords.fields.x = ai->max_possibilities;
                            ai->current_action.coords.fields.y = ai->y_counter;
                            ai->current_action.coords.fields.elevation = ai->map_level_counter;
                            battle_ai_finalize_action_destination();
                        evaluate_scanned_origin:
                            if (battle_ai_evaluate_target_tiles() == -1) {
                                ai->highest_priority_state = AI_ORIGIN_EVALUATION_SCANNED;
                                return -1;
                            }
                        }
                    }
                }
            }
            ai->map_level_counter++;
        } while (ai->map_level_counter < MAP_TERRAIN_LAYER_COUNT);
    }
    battle_ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    return 0;
}

/*
 * Mark an actor-origin choice and check the resulting tile for crystal or treasure.
 *
 * An actor-origin choice sets act_before_move and substitutes the movement
 * scenario's candidate coordinates; other origins clear that flag.
 */
void battle_ai_finalize_action_destination(void) {
    ai_data_t* ai;

    ai = &g_ai_data_base;
    if (g_ai_data_base.current_action.coords.word != g_ai_data_base.acting_unit_coords.word) {
        g_ai_data_base.current_action.act_before_move = 0;
    } else {
        g_ai_data_base.current_action.act_before_move = 1;
        g_ai_data_base.current_action.coords = ai->candidate_coords[g_ai_data_base.movement_scenario];
    }
    battle_ai_update_tile_pickup();
}

/*
 * Take the lowest-key remaining movement origin for target evaluation.
 *
 * Rank by signed tile priority, then foe proximity; later tiles win ties.
 * Requires a consistent nonempty movable mask/count; there is no empty guard.
 */
void battle_ai_take_next_movement_origin(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 best_key = 0x7fffffff;
    s32 x, y, level;
    s32 key;
    u16 row;

    for (level = 0; level < MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            row = ai->movable_tiles[level][y];
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if ((row << x) & 0x8000) {
                        s32 priority_scale = 65536;
                        key = ai->tile_priority[level][y][x];
                        key *= priority_scale;
                        key += ai->tile_foe_proximity[level][y][x];
                        if (best_key >= key) {
                            best_key = key;
                            ai->current_action.coords.fields.x = x;
                            ai->current_action.coords.fields.y = y;
                            ai->current_action.coords.fields.elevation = level;
                        }
                    }
                }
            }
        }
    }
    ai->movable_tiles[ai->current_action.coords.fields.elevation][ai->current_action.coords.fields.y]
        ^= 0x8000 >> ai->current_action.coords.fields.x;
    ai->movable_tile_count--;
}

/* Build attack-origin and movable tile masks for the considered ability. */
void battle_ai_build_attack_origin_masks(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 i;
    s32 j;
    s32 x;
    s32 unit_id;
    s32 depth;
    unit_stats_t* unit;
    unit_stats_t* pos;
    s32 jumping;
    u8 status;

    for (i = 0; i < AI_GRID_ROW_COUNT; i++) {
        ai->attack_origin_tiles[0][i] = 0;
        ai->attack_origin_tiles[1][i] = 0;
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_ARC_TRAJECTORY) {
        battle_mem_copy_u16_values(
            ai->movable_tiles, ai->reachable_tiles[ai->movement_scenario][0], sizeof(ai->movable_tiles));
    } else {
        for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
            if (ai->targetability.live.unit_targetable[unit_id]) {
                battle_ai_copy_unit_coordinates(unit_id, &ai->considered_unit_coords);
                if (ai->current_action.command.targeting_type == BATTLE_TARGETING_TYPE_UNIT)
                    battle_ai_build_targetable_tile_mask(AI_TILE_MASK_RANGE, 0);
                else
                    battle_ai_build_targetable_tile_mask(AI_TILE_MASK_RANGE_PLUS_AOE, 0);
                for (i = 0; i < ai->map_max_y; i++) {
                    ai->attack_origin_tiles[0][i] |= ai->targetable_tiles[0][i];
                    ai->attack_origin_tiles[1][i] |= ai->targetable_tiles[1][i];
                }
            }
        }
        for (i = 0; i < ai->map_max_y; i++) {
            ai->movable_tiles[0][i] = ai->reachable_tiles[ai->movement_scenario][0][i] & ai->attack_origin_tiles[0][i];
            ai->movable_tiles[1][i] = ai->reachable_tiles[ai->movement_scenario][1][i] & ai->attack_origin_tiles[1][i];
            ai->attack_origin_tiles[0][i] = 0;
            ai->attack_origin_tiles[1][i] = 0;
        }
        for (i = 0; i < MAP_TERRAIN_LAYER_COUNT; i++) {
            for (j = 0; j < ai->map_max_y; j++) {
                unit_id = ai->movable_tiles[i][j];
                if (unit_id == 0)
                    continue;
                for (x = 0; x < ai->map_max_x; x++) {
                    if ((unit_id << x) & 0x8000) {
                        if (ai->water_penalty
                            && (depth = g_map_tile_data[j * ai->map_max_x + x + (i * MAP_TILES_PER_LAYER)]
                                       .depth_half_height.depth)
                                >= 2) {
                            ai->movable_tiles[i][j] ^= 0x8000 >> x;
                        } else if (g_main_action_menu_types_by_skillset[ai->considered_ability.skillset]
                            == ACTION_MENU_TYPE_ELEMENTS) {
                            ai->considered_unit_coords.fields.x = x;
                            ai->considered_unit_coords.fields.y = j;
                            ai->considered_unit_coords.fields.elevation = i;
                            if (!battle_ai_can_use_geomancy(&ai->considered_unit_coords))
                                ai->movable_tiles[i][j] ^= 0x8000 >> x;
                        }
                    }
                }
            }
        }
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_REQUIRES_MONSTER_SKILL) {
            battle_ai_build_monster_skill_tile_mask();
            for (i = 0; i < ai->map_max_y; i++) {
                ai->movable_tiles[0][i] &= ai->targetable_tiles[0][i];
                ai->movable_tiles[1][i] &= ai->targetable_tiles[1][i];
            }
        }
    }
    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        if (ai->targetability.live.unit_targetable[unit_id]) {
            battle_ai_copy_unit_coordinates(unit_id, &ai->considered_unit_coords);
            if (ai->current_action.command.targeting_type == BATTLE_TARGETING_TYPE_UNIT)
                battle_ai_build_targetable_tile_mask(AI_TILE_MASK_EXPLICIT, 0);
            else
                battle_ai_build_targetable_tile_mask(AI_TILE_MASK_AOE, 0);
            for (i = 0; i < ai->map_max_y; i++) {
                ai->attack_origin_tiles[0][i] |= ai->targetable_tiles[0][i];
                ai->attack_origin_tiles[1][i] |= ai->targetable_tiles[1][i];
            }
        }
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_FOLLOW_TARGET) {
        for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
            if (ai->targetability.live.unit_targetable[unit_id]) {
                unit = &g_battle_unit_stats[unit_id];
                status = unit->status_sets.current[0];
                /* Keeps the position reads on a copy of the loop pointer (`move a1,a2`) that cse2 would fold. */
                pos = unit;
                __asm__("" : "=r"(pos) : "0"(pos));
                jumping = status & STATUS_BYTE_MASK(STATUS_ID_JUMP);
                if (ai->current_action.command.targeting_type == BATTLE_TARGETING_TYPE_TILE || jumping) {
                    ai->attack_origin_tiles[pos->position.higher_elevation][pos->position.y] ^= 0x8000 >> pos->x;
                }
            }
        }
    }
    for (i = 0; i < MAP_TERRAIN_LAYER_COUNT; i++) {
        for (j = 0; j < ai->map_max_y; j++) {
            ai->attack_origin_tiles[i][j] &= ai->walkable_tiles[i][j];
        }
    }
    ai->attack_origin_tile_count = battle_ai_count_flagged_tiles_within_map_bounds(ai->attack_origin_tiles);
    ai->movable_tile_count = battle_ai_count_flagged_tiles_within_map_bounds(ai->movable_tiles);
}

/*
 * Build the considered origin's targetable-tile mask on both map levels.
 *
 * Modes 0, 1, and 2 select range, area, or their sum; other modes use radius.
 * Radius 16 or greater fills both masks without applying the linear filter.
 * Radii 8 through 15 read the following status-priority weights instead of
 * row templates. Jump Level 8 can supply radius 8 on non-arcing origin scans.
 * The lookup is unchecked.
 */
void battle_ai_build_targetable_tile_mask(ai_tile_mask_mode_e mode, s32 radius) {
    ai_data_t* ai = &g_ai_data_base;
    s32 i;
    s32 x;
    s32 y;

    /* Preserve the target's signed mode dispatch. */
    switch ((s32)mode) {
    case AI_TILE_MASK_RANGE:
        radius = g_ai_data_base.considered_ability.parameters.fields.range;
        break;
    case AI_TILE_MASK_AOE:
        radius = g_ai_data_base.considered_ability.parameters.fields.aoe;
        break;
    case AI_TILE_MASK_RANGE_PLUS_AOE:
        radius = g_ai_data_base.considered_ability.parameters.fields.range
            + g_ai_data_base.considered_ability.parameters.fields.aoe;
        break;
    }
    if (radius >= 16) {
        for (i = 0; i < AI_GRID_ROW_COUNT; i++) {
            ai->targetable_tiles[0][i] = 0xffff;
            ai->targetable_tiles[1][i] = 0xffff;
        }
        return;
    }
    for (i = AI_GRID_ROW_COUNT - 1; i >= 0; i--)
        ai->targetable_tiles[0][i] = 0;
    y = ai->considered_unit_coords.fields.y;
    x = ai->considered_unit_coords.fields.x;
    for (i = 0; i <= radius; i++) {
        if (y - i < 0)
            break;
        if (x < 8)
            ai->targetable_tiles[0][y - i] = g_ai_range_row_masks[radius - i] << (7 - x);
        else
            ai->targetable_tiles[0][y - i] = g_ai_range_row_masks[radius - i] >> (x - 7);
    }
    for (i = 1; i <= radius; i++) {
        if (y + i >= ai->map_max_y)
            break;
        if (x < 8)
            ai->targetable_tiles[0][y + i] = g_ai_range_row_masks[radius - i] << (7 - x);
        else
            ai->targetable_tiles[0][y + i] = g_ai_range_row_masks[radius - i] >> (x - 7);
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_LINEAR_TRAJECTORY) {
        for (y = 0; y < ai->map_max_y; y++) {
            i = ai->targetable_tiles[0][y];
            if (i != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if ((i << x) & 0x8000) {
                        if (x != ai->considered_unit_coords.fields.x && y != ai->considered_unit_coords.fields.y)
                            ai->targetable_tiles[0][y] ^= 0x8000 >> x;
                    }
                }
            }
        }
    }
    battle_mem_copy_u16_values(ai->targetable_tiles[1], ai->targetable_tiles[0], sizeof(ai->targetable_tiles[0]));
}

/* The target loads 0xff as 255, not -1, for the first 2 directions. */
/*
 * Build the targetable-tile mask around same-team Monster Skill holders.
 *
 * Only the low map level is visited; the high-level mask remains cleared.
 * Tile base heights must differ by at most 2; slope and depth are ignored.
 */
void battle_ai_build_monster_skill_tile_mask(void) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit;
    s32 i;
    s32 direction;
    s32 x;
    s32 y;
    s32 level;
    s32 height;
    u16* row;
    u32 level_base;

    for (level = 0; level < AI_GRID_ROW_COUNT; level++) {
        ai->targetable_tiles[0][level] = 0;
        ai->targetable_tiles[1][level] = 0;
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
            continue;
        if (!(unit->ability_flags.support[3] & BATTLE_SUPPORT_SET_4_MONSTER_SKILL))
            continue;
        if ((unit->initial_team_flags & BATTLE_TEAM_MASK) != ai->acting_unit_team)
            continue;
        if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_PREVENT_REACTION))
            continue;
        height = g_map_tile_data[unit->position.higher_elevation * MAP_TILES_PER_LAYER
            + unit->position.y * ai->map_max_x + unit->x]
                     .height;
        for (direction = 0; direction < AI_CARDINAL_DIRECTION_COUNT; direction++) {
            x = unit->x + g_ai_facing_tile_offsets.by_direction[direction][0];
            if (x < 0 || x >= ai->map_max_x)
                continue;
            y = unit->position.y + g_ai_facing_tile_offsets.by_direction[direction][1];
            if (y < 0 || y >= ai->map_max_y)
                continue;
            for (level = 0; level <= 0; level++) {
                if ((g_map_tile_data[(level * MAP_TILES_PER_LAYER) + y * ai->map_max_x + x].height - height) >= -2
                    && (g_map_tile_data[(level * MAP_TILES_PER_LAYER) + y * ai->map_max_x + x].height - height) <= 2) {
                    /* A typed level-row pointer folds the field offset into
                     * the induction value. Preserve the target's AI-base
                     * cursor with a member-derived displacement. */
                    level_base = level * sizeof(ai->targetable_tiles[0]) + (u32)ai;
                    row = (u16*)(level_base + ((u32)&ai->targetable_tiles - (u32)ai));
                    row[y] |= 0x8000 >> x;
                }
            }
        }
    }
}

/*
 * Score target tiles from the current simulated action origin.
 *
 * Intersect range with remaining target candidates, apply direct/arc path checks,
 * and resume suspended priority evaluation at its saved tile. A -1 result
 * suspends this pass; 0 ends it. Coordinate restoration belongs to callers.
 */
s32 battle_ai_evaluate_target_tiles(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 x, y, level;
    u16 row;
    ai_coords_t* origin;
    s32 i;
    map_tile_t* tile;
    u16* target_rows;
    u16* origin_rows;
    s32 tile_mask;
    u32 level_address;
    u8 target_id;

    if (g_ai_data_base.resume_pending && g_ai_data_base.search_resume_flag) {
        x = (u16)g_ai_data_base.current_action.command.target_position.vx;
        level = (u16)g_ai_data_base.current_action.command.target_position.vy;
        y = (u16)g_ai_data_base.current_action.command.target_position.vz;
        row = ai->targetable_tiles[level][y];
        goto evaluate_priority;
    }
    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        ai->search_resume_flag = 0;
        return -1;
    }
    ai->resume_pending = 0;
    if (ai->current_action.act_before_move)
        origin = &ai->acting_unit_coords;
    else
        origin = &ai->current_action.coords;
    battle_ai_move_temp_unit_to_coords(origin);
    if (ai->considered_ability.parameters.fields.range) {
        if (battle_target_set_panels_for_action(&ai->current_action.command) == TARGET_PANELS_NONE)
            return 0;
    } else {
        tile = g_map_tile_data;
        for (i = 0; i < MAP_TILE_COUNT; i++, tile++)
            tile->ceiling_depth_and_marks.ability_range = 0;
        g_map_tile_data[origin->fields.y * ai->map_max_x + origin->fields.x
            + (origin->fields.elevation * MAP_TILES_PER_LAYER)]
            .ceiling_depth_and_marks.ability_range = 1;
    }

    /* Field-derived level bases keep both row pointers in the target order;
     * typed level indexing adds instructions to this intersection loop. */
    for (level = 0, level_address = (u32)ai; level < MAP_TERRAIN_LAYER_COUNT;
        level++, level_address += AI_GRID_ROW_COUNT * sizeof(u16)) {
        for (y = 0; y < ai->map_max_y; y++) {
            target_rows = (u16*)(level_address + ((u32)&ai->targetable_tiles - (u32)ai));
            origin_rows = (u16*)(level_address + ((u32)&ai->attack_origin_tiles - (u32)ai));
            row = origin_rows[y];
            target_rows[y] = 0;
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    /* Keep the membership test and update in one working mask. */
                    tile_mask = row << x;
                    if (tile_mask & 0x8000) {
                        if (g_map_tile_data[y * ai->map_max_x + x + (level * MAP_TILES_PER_LAYER)]
                                .ceiling_depth_and_marks.ability_range) {
                            tile_mask = 0x8000;
                            target_rows[y] |= tile_mask >> x;
                        }
                    }
                }
            }
        }
    }
    for (level = 0; level < MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            row = ai->targetable_tiles[level][y];
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if ((row << x) & 0x8000) {
                        ai->current_action.command.target_position.vx = x;
                        ai->current_action.command.target_position.vz = y;
                        ai->current_action.command.target_position.vy = level;
                        if (ai->current_action.command.targeting_type == BATTLE_TARGETING_TYPE_UNIT
                            || (ai->considered_ability.ai_flags.word
                                & (AI_ABILITY_DIRECT_TRAJECTORY | AI_ABILITY_ARC_TRAJECTORY))) {
                            target_id = battle_ai_find_unit_on_target_panel();
                            ai->current_action.command.target_id = target_id;
                            if (target_id == BATTLE_UNIT_ID_NONE)
                                goto remove_useful_origin;
                        }
                        if (ai->considered_ability.ai_flags.word & AI_ABILITY_ARC_TRAJECTORY) {
                            if (battle_effect_check_arc_trajectory_between_units(
                                    ai->acting_unit_id, ai->current_action.command.target_id)
                                == 0)
                                continue;
                        } else if (ai->considered_ability.ai_flags.word & AI_ABILITY_DIRECT_TRAJECTORY) {
                            if ((g_main_action_menu_types_by_skillset[ai->considered_ability.skillset]
                                        != ACTION_MENU_TYPE_ITEM_INVENTORY
                                    || battle_ai_calculate_distance_between_units(
                                           ai->acting_unit, &g_battle_unit_stats[ai->current_action.command.target_id])
                                        >= 2)
                                && battle_effect_check_direct_trajectory_between_units(
                                       ai->acting_unit_id, ai->current_action.command.target_id)
                                    == 0)
                                continue;
                        }
                        if (ai->targetability.live.target_setting_flags & AI_TARGET_SETTING_CONSUME_EVALUATED_ORIGINS)
                            ai->useful_on_caster_flag = 1;
                        else
                            ai->useful_on_caster_flag = 0;
                    evaluate_priority:
                        if (battle_ai_evaluate_current_action() == -1) {
                            ai->search_resume_flag = 1;
                            return -1;
                        }
                        if (ai->ability_processing_done == 0)
                            return 0;
                    remove_useful_origin:
                        if (ai->useful_on_caster_flag) {
                            ai->attack_origin_tiles[level][y] ^= 0x8000 >> x;
                            ai->attack_origin_tile_count--;
                            if (ai->attack_origin_tile_count == 0)
                                break;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

/* Index of the first present unit standing on the current action's target
 * panel, or BATTLE_UNIT_ID_NONE if none. */
s32 battle_ai_find_unit_on_target_panel(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 i;
    unit_stats_t* unit;
    u16 x;
    u16 y;
    u16 elevation;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            continue;
        }
        x = ai->current_action.command.target_position.vx;
        if (unit->x != x) {
            continue;
        }
        y = ai->current_action.command.target_position.vz;
        if (unit->position.y != y) {
            continue;
        }
        elevation = ai->current_action.command.target_position.vy;
        if (unit->position.higher_elevation == elevation) {
            break;
        }
    }
    return i;
}

/* Return the first present unit's slot at these coordinates, or BATTLE_UNIT_ID_NONE if absent. */
s32 battle_ai_find_unit_at_coordinates(const ai_coords_t* coords) {
    s32 i;
    unit_stats_t* unit;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && unit->x == coords->fields.x
            && unit->position.y == coords->fields.y && (unit->position.higher_elevation) == coords->fields.elevation) {
            break;
        }
    }
    return i;
}

/* Simulate and score the current action and keep it as the best when it
 * outranks the best so far. -1 suspends; otherwise 0. */
s32 battle_ai_evaluate_current_action(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 result = battle_ai_simulate_action_and_score();

    if (result == -1) {
        return -1;
    }
    if (result != 0) {
        if (battle_ai_is_action_higher_ranked(&ai->current_action, &ai->best_action) != 0) {
            battle_mem_copy_u16_values(&ai->best_action, &ai->current_action, sizeof(ai_action_data_t));
        }
    }
    return 0;
}
