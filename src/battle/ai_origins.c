/* AI attack origins, target evaluation and action simulation, with the math ability table. */

#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/libetc.h"
#include "psx/types.h"

static void battle_ai_build_monster_skill_tile_mask(void);
static void battle_ai_build_targetable_tile_mask(ai_tile_mask_mode_e mode, s32 radius);
static s32 battle_ai_can_status_cancellation_affect_turn(const unit_stats_t* unit, s32 status);
static s32 battle_ai_can_unit_act_within_charge_time(s32 unit_index);
static s32 battle_ai_can_use_geomancy(const ai_coords_t* coords);
static void battle_ai_clear_current_action_decision(void);
static s32 battle_ai_evaluate_math_targets(void);
static s32 battle_ai_evaluate_movement_origins(void);
static s32 battle_ai_evaluate_reflected_target_origins(void);
static s32 battle_ai_evaluate_self_target_origins(void);
static s32 battle_ai_evaluate_target_tiles(void);
static s32 battle_ai_exclude_enemies_acting_before_ct(void);
static s32 battle_ai_filter_useful_targets(void);
static void battle_ai_finalize_action_destination(void);
static s32 battle_ai_find_unit_on_target_panel(void);
static s32 battle_ai_is_facing_tile_blocked(const ai_coords_t* source, const ai_coords_t* target);
static s32 battle_ai_is_unit_active_after_delay(s32 limit, s32 unit_id);
static void battle_ai_restore_unit_state(ai_snapshot_mode_e mode);
static void battle_ai_save_unit_state(ai_snapshot_mode_e mode);
static s32 battle_ai_simulate_action_hits(action_command_t* action);
static void battle_ai_simulate_movement_and_pickup(void);
static void battle_ai_simulate_strikes(action_command_t* action, s32 reaction_mode);
static void battle_ai_take_next_movement_origin(void);

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
static s32 battle_ai_is_facing_tile_blocked(const ai_coords_t* source, const ai_coords_t* target) {
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

/* g_ai_math_extra_known_ability_ids: read-only table of this object (12 bytes at 0x80193dd4), kept byte for byte
 * until it is written as its typed definition. */
static const u32 g_ai_math_extra_known_ability_ids_rodata[3] = {
    0x10050201,
    0x18151411,
    0x00002a19,
};

/*
 * Evaluate self-target alternatives from the actor and candidate coordinates.
 *
 * Record the resume stage when returning -1 to suspend. Completed searches
 * restore the actor coordinates and submit the action to the ranked list.
 */
static s32 battle_ai_evaluate_self_target_origins(void) {
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
static s32 battle_ai_can_use_geomancy(const ai_coords_t* coords) {
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
static s32 battle_ai_evaluate_reflected_target_origins(void) {
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
static s32 battle_ai_evaluate_math_targets(void) {
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
static s32 battle_ai_evaluate_movement_origins(void) {
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
static void battle_ai_finalize_action_destination(void) {
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
static void battle_ai_take_next_movement_origin(void) {
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
static void battle_ai_build_targetable_tile_mask(ai_tile_mask_mode_e mode, s32 radius) {
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
static void battle_ai_build_monster_skill_tile_mask(void) {
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
static s32 battle_ai_evaluate_target_tiles(void) {
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
static s32 battle_ai_find_unit_on_target_panel(void) {
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

enum {
    AI_HP_SCORE_SCALE = 128,
    AI_MP_SCORE_SCALE = 64,
    AI_EQUIPMENT_SCORE = 51,
    AI_GOLEM_SCORE_SCALE = 128,
};

/*
 * Simulate the current action and intervening events, then score unit state.
 *
 * Return -1 to suspend, 0 for a rejected or flag-update-only pass, and 1
 * after scoring. Save/restore helpers preserve selected unit data, not the
 * whole AI workspace; resumed phases skip fresh action preparation.
 */
s32 battle_ai_simulate_action_and_score(void) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit;
    ai_unit_decision_t* decision;
    action_command_t* action;
    ai_data_t* clear_cursor;
    ai_data_t* decision_base;
    s32 unit_index;
    s32 status;
    s32 equipment_slot;
    s32 weight_offset;
    s32 weight;
    s32 score;
    u32 below_cost;

    if (g_ai_data_base.resume_pending && g_ai_data_base.action_simulation_phase) {
        battle_ai_restore_unit_state(AI_SNAPSHOT_SIMULATED_STATUS);
        /* Resume: re-enter the suspended turn-advance or scoring phase. */
        if (g_ai_data_base.action_simulation_phase == AI_SIMULATION_ADVANCE_TURNS)
            goto advance_turns;
        if (g_ai_data_base.action_simulation_phase == AI_SIMULATION_SCORE_STATE)
            goto score_state;
    }
    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        ai->action_simulation_phase = AI_SIMULATION_PREPARE_ACTION;
        return -1;
    }
    battle_ai_save_unit_state(AI_SNAPSHOT_BASELINE);
    ai->ability_processing_done = 0;
    /* Direct array iteration changes the bytes; retain a workspace-relative cursor. */
    for (unit_index = BATTLE_UNIT_SLOT_COUNT - 1, clear_cursor = (ai_data_t*)((u8*)ai + (BATTLE_UNIT_SLOT_COUNT - 1));
        unit_index >= 0; unit_index--, clear_cursor = (ai_data_t*)((u8*)clear_cursor - 1)) {
        clear_cursor->unit_acts_before_me[0] = 0;
    }
    ai->current_action.rank.priority = 0;
    ai->current_action.rank.base_hit_percent = 0xff;
    if (ai->current_action.act_before_move) {
        if (ai->current_action.command.skillset_id != 0
            && ai->current_action.command.skillset_id != SKILLSET_ID_DEFEND) {
            action = &ai->current_action.command;
            if (battle_action_call_attack_preparation_at_preview(action) == BATTLE_ATTACK_PREPARATION_IMMEDIATE
                && battle_ai_simulate_action_hits(action) == 0) {
                battle_ai_restore_unit_state(AI_SNAPSHOT_BASELINE);
                ai->resume_pending = 0;
                return 0;
            }
            ai->acting_unit->action_taken = 1;
            battle_ai_simulate_movement_and_pickup();
        }
    } else {
        battle_ai_simulate_movement_and_pickup();
        if (ai->current_action.command.skillset_id != 0
            && ai->current_action.command.skillset_id != SKILLSET_ID_DEFEND) {
            action = &ai->current_action.command;
            if (battle_action_call_attack_preparation_at_preview(action) == BATTLE_ATTACK_PREPARATION_IMMEDIATE
                && battle_ai_simulate_action_hits(action) == 0) {
                battle_ai_restore_unit_state(AI_SNAPSHOT_BASELINE);
                ai->resume_pending = 0;
                return 0;
            }
            ai->acting_unit->action_taken = 1;
        }
    }
    if (ai->current_action.command.skillset_id == SKILLSET_ID_DEFEND) {
        battle_status_inflict_defending_to_battle_id(ai->acting_unit_id);
        ai->current_action.rank.base_hit_percent = 100;
        ai->acting_unit->action_taken = 1;
    }
    if (ai->current_action.command.skillset_id == 0)
        ai->current_action.rank.base_hit_percent = 100;
    battle_action_end_turn(ai->acting_unit_id);
    ai->saved_between_turn_state = g_battle_between_turn_state;
    ai->saved_between_turn_resume_state = g_battle_between_turn_resume_state;
    ai->saved_turn_unit_id = g_battle_turn_unit_id;
    ai->simulated_turn_events = 0;
    for (;;) {
    advance_turns:
        if (VSync(1) >= AI_TURN_SIMULATION_SCANLINE_LIMIT) {
            battle_ai_save_unit_state(AI_SNAPSHOT_SIMULATED_STATUS);
            ai->action_simulation_phase = AI_SIMULATION_ADVANCE_TURNS;
            return -1;
        }
        ai->simulated_turn_events++;
        if (ai->simulated_turn_events >= AI_SIMULATED_TURN_LIMIT)
            break;
        unit_index = battle_action_run_between_turn_control(BATTLE_BETWEEN_TURN_MODE_RUN);
        status = unit_index & 0xff00;
        unit_index &= 0xff;
        unit = &g_battle_unit_stats[unit_index];
        if (status == BATTLE_TURN_EVENT_NONE)
            break;
        if (status == BATTLE_TURN_EVENT_ACTION_RESULT) {
            battle_action_finalize_attack_and_flag_reactions(unit_index);
        } else if (status == BATTLE_TURN_EVENT_ABILITY_READY) {
            if (battle_ai_simulate_action_hits(&unit->action_command) == 0) {
                g_battle_between_turn_state = ai->saved_between_turn_state;
                g_battle_between_turn_resume_state = ai->saved_between_turn_resume_state;
                g_battle_turn_unit_id = ai->saved_turn_unit_id;
                /* An early return duplicates this shared cleanup tail. */
                goto finish_without_scoring;
            }
            unit->charged_ability_ct = 0xff;
            unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                &= ~(STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
            unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                &= ~(STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
        } else if (status == BATTLE_TURN_EVENT_UNIT_READY) {
            if (battle_ai_is_unit_active_after_delay(0, unit_index))
                ai->unit_acts_before_me[unit_index] = 1;
            unit->has_turn = 0;
        }
        for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
            unit = &g_battle_unit_stats[unit_index];
            if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                    & (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING))))
                break;
        }
        if (unit_index == BATTLE_UNIT_ID_NONE)
            break;
    }
    g_battle_between_turn_state = ai->saved_between_turn_state;
    g_battle_between_turn_resume_state = ai->saved_between_turn_resume_state;
    g_battle_turn_unit_id = ai->saved_turn_unit_id;
score_state:
    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        battle_ai_save_unit_state(AI_SNAPSHOT_SIMULATED_STATUS);
        ai->action_simulation_phase = AI_SIMULATION_SCORE_STATE;
        return -1;
    }
    if (ai->targetability.live.target_setting_flags & AI_TARGET_SETTING_REFRESH_UNIT_STATUS_FLAGS) {
        for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
            decision = &ai->unit_decisions[unit_index];
            unit = &g_battle_unit_stats[unit_index];
            decision->flags
                &= ~(AI_DECISION_TRANSIENT_DEAD | AI_DECISION_TRANSIENT_PETRIFIED | AI_DECISION_TRANSIENT_JUMP);
            if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
                && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)]
                    & STATUS_BYTE_MASK(STATUS_ID_RERAISE)))
                decision->flags |= AI_DECISION_TRANSIENT_DEAD;
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
                decision->flags |= AI_DECISION_TRANSIENT_PETRIFIED;
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)] & STATUS_BYTE_MASK(STATUS_ID_JUMP))
                decision->flags |= AI_DECISION_TRANSIENT_JUMP;
            if (!(decision->flags & AI_DECISION_CONSIDERED)
                && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
                    & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
                && (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
                    || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)]
                        & STATUS_BYTE_MASK(STATUS_ID_RERAISE)))
                && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
                    & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE)))
                decision->flags |= AI_DECISION_CONSIDERED;
        }
    } else {
        battle_ai_clear_current_action_decision();
        for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
            status = ai->unit_compact_indices[unit_index];
            if (status == 0xff)
                continue;
            unit = &g_battle_unit_stats[unit_index];
            score = unit->hp;
            score *= AI_HP_SCORE_SCALE;
            score /= unit->max_hp;
            status = 0;
            /* A pointer to unit_decisions adds an addiu and changes scheduling;
             * retain the workspace-relative stride. */
            decision_base = (ai_data_t*)((u8*)ai + unit_index * sizeof(ai_unit_decision_t));
            for (weight_offset = 0; status < STATUS_COUNT; status++, weight_offset += 2) {
                if (!((unit->status_sets.current[status / 8] << (status % 8)) & 0x80))
                    continue;
                /* Indexing the s16 table changes register allocation; retain the byte cursor. */
                weight = *(s16*)((u8*)g_ai_status_priority_weights + weight_offset);
                if (((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
                         & (STATUS_BYTE_MASK(STATUS_ID_CONFUSION) | STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)))
                        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)]
                            & STATUS_BYTE_MASK(STATUS_ID_CHARM)))
                    && weight >= 0)
                    continue;
                switch (status) {
                case STATUS_ID_DARKNESS:
                    weight = weight * decision_base->unit_decisions[0].evade_mod / 4;
                    break;
                case STATUS_ID_SILENCE:
                    weight = weight * decision_base->unit_decisions[0].silence_mod / 4;
                    break;
                case STATUS_ID_CONFUSION:
                case STATUS_ID_BLOOD_SUCK:
                case STATUS_ID_CHARM:
                    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLOW)]
                            & (STATUS_BYTE_MASK(STATUS_ID_SLOW) | STATUS_BYTE_MASK(STATUS_ID_STOP)))
                        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)]
                            & (STATUS_BYTE_MASK(STATUS_ID_SLEEP) | STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE)
                                | STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))))
                        weight /= 2;
                    break;
                case STATUS_ID_SLOW:
                case STATUS_ID_STOP:
                case STATUS_ID_SLEEP:
                case STATUS_ID_DONT_MOVE:
                case STATUS_ID_DONT_ACT:
                    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
                            & (STATUS_BYTE_MASK(STATUS_ID_CONFUSION) | STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)))
                        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)]
                            & STATUS_BYTE_MASK(STATUS_ID_CHARM)))
                        continue;
                    break;
                }
                score += weight;
            }
            if (!(unit->unit_flags & UNIT_FLAG_MONSTER)) {
                for (equipment_slot = 0; equipment_slot < UNIT_EQUIPMENT_SLOT_COUNT; equipment_slot++) {
                    if (unit->equipment[equipment_slot]
                        == g_ai_unit_snapshots[ai->unit_compact_indices[unit_index]].equipment[equipment_slot])
                        score += AI_EQUIPMENT_SCORE;
                }
            }
            if (unit->max_mp) {
                weight_offset = unit->mp;
                below_cost = weight_offset;
                weight_offset *= AI_MP_SCORE_SCALE;
                below_cost = below_cost < ai->unit_decisions[unit_index].lowest_mp_cost;
                weight_offset /= unit->max_mp;
                if (below_cost)
                    weight_offset = (u32)weight_offset >> 1;
                weight_offset = weight_offset * ai->unit_decisions[unit_index].mp_ability_mod / 4;
                score += weight_offset;
            }
            {
                u32 golem;

                weight_offset = g_battle_team_golem[(unit->team_flags >> BATTLE_TEAM_SHIFT) & BATTLE_TEAM_INDEX_MASK];
                golem = weight_offset;
                weight_offset *= AI_GOLEM_SCORE_SCALE;
                score += weight_offset / ai->present_unit_average_max_hp;
                if (golem < ai->team_golem[(unit->team_flags >> BATTLE_TEAM_SHIFT) & BATTLE_TEAM_INDEX_MASK])
                    score--;
            }
            if (ai->unit_decisions[unit_index].enemy_flag)
                score = -score;
            ai->current_action.rank.priority += score;
        }
        if (g_main_action_menu_types_by_skillset[ai->considered_ability.skillset] == ACTION_MENU_TYPE_WEAPON_INVENTORY
            && ai->considered_ability.ability_id != ABILITY_ID_THROW_SHURIKEN
            && ai->considered_ability.ability_id != ABILITY_ID_THROW_BALL)
            ai->current_action.rank.base_hit_percent >>= 1;
        battle_ai_restore_unit_state(AI_SNAPSHOT_BASELINE);
        ai->resume_pending = 0;
        return 1;
    }
finish_without_scoring:
    battle_ai_restore_unit_state(AI_SNAPSHOT_BASELINE);
    ai->resume_pending = 0;
    return 0;
}

/*
 * Simulate movement and its crystal or treasure benefit.
 *
 * An unchanged 32-bit coordinate value skips all updates. Crystal recovery clears
 * Critical; a noncritical pickup adds priority unless the policy is Retreat.
 */
static void battle_ai_simulate_movement_and_pickup(void) {
    ai_data_t* ai = &g_ai_data_base;

    if (ai->current_action.coords.word != ai->acting_unit_coords.word) {
        battle_ai_move_temp_unit_to_coords(&ai->current_action.coords);
        if (ai->tile_pickup_kind == AI_TILE_PICKUP_CRYSTAL) {
            ai->acting_unit->hp = ai->acting_unit->max_hp;
            ai->acting_unit->mp = ai->acting_unit->max_mp;
            ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)]
                &= ~STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
            ai->acting_unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)]
                &= ~STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
        }
        if (ai->tile_pickup_kind != AI_TILE_PICKUP_NONE
            && !(ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)]
                & STATUS_BYTE_MASK(STATUS_ID_CRITICAL))
            && ai->autobattle_setting != AI_AUTOBATTLE_RETREAT) {
            ai->current_action.rank.priority += 2;
        }
        ai->acting_unit->movement_taken = 1;
    }
}

/* Mark the candidate action as not decided on: only the selected action ever
 * holds AI_ACTION_DECIDED (ai_build_command). */
static void battle_ai_clear_current_action_decision(void) {
    g_ai_data_base.current_action.rank.decided = 0;
}

/*
 * Simulate an action and finalize its hit-percentage average when requested.
 *
 * Accounting starts only for the acting unit with an unset base hit rate.
 * The 0xff counter disables accounting; that path returns 1 without
 * requiring a valuable target. Counted entries are target/strike pairs.
 */
static s32 battle_ai_simulate_action_hits(action_command_t* action) {
    ai_data_t* ai;
    s32 base_hit;
    s32 cnt;

    ai = &g_ai_data_base;
    g_ai_data_base.hit_counter = 0xFF;
    if (action->actor_id == g_ai_data_base.acting_unit_id) {
        base_hit = g_ai_data_base.current_action.rank.base_hit_percent;
        g_ai_data_base.ability_processing_done = 1;
        if (base_hit == 0xFF) {
            g_ai_data_base.hit_counter = 0;
            g_ai_data_base.total_hit_percent = 0;
            g_ai_data_base.valuable_target_hit = 0;
        }
    }
    battle_ai_simulate_strikes(action, 0);
    cnt = ai->hit_counter;
    if (cnt != 0xFF) {
        if (ai->valuable_target_hit == 0 || cnt == 0) {
            return 0;
        }
        ai->current_action.rank.base_hit_percent = ai->total_hit_percent / cnt;
    }
    return 1;
}

/*
 * Simulate strikes and eligible reactions, updating AI hit accounting.
 *
 * A nonzero reaction mode skips initial preparation and recursive reaction dispatch.
 * Hit accounting is disabled by counter 0xff; included target/strike pairs
 * can adjust priority and mark whether a valuable target was hit.
 */
static void battle_ai_simulate_strikes(action_command_t* action, s32 reaction_mode) {
    ai_data_t* ai = &g_ai_data_base;
    battle_strike_work_t work;
    s32 target_index;
    s32 target_id;
    action_data_t* result;
    ability_ai_flags_t* behavior;
    s32 self_hit;
    s32 positive;
    s32 negative;

    if (reaction_mode == 0 && battle_action_init_current_data(action->actor_id) != 0) {
        return;
    }
    behavior = &g_main_ability_data[action->ability_id].ai_flags;
    if (g_ai_data_base.hit_counter != 0xff) {
        self_hit = 0;
        positive = 0;
        negative = 1;
        if (behavior->flags_1 & AI_ABILITY_AFFECTS_STATS) {
            positive = 3;
            negative = 4;
        }
    }
    do {
        if (battle_action_resolve_ability_strike(action->actor_id, &work) == 0 && work.target_count != 0) {
            /* The target scans only for 0xff, without a count or 16-entry bound. */
            for (target_index = 0; work.target_list[target_index] != 0xff; target_index++) {
                target_id = work.target_list[target_index];
                result = &g_battle_unit_stats[target_id].action;
                if (result->attack_accuracy <= 0) {
                    continue;
                }
                if (!(behavior->flags_2 & AI_ABILITY_FLAG_2_TARGET_MAP_TILES)) {
                    if (battle_ai_is_unit_untargetable(target_id) != 0) {
                        continue;
                    }
                    if (action->targeting_type == BATTLE_TARGETING_TYPE_UNIT
                        && ai->unit_acts_before_me[action->target_id] != 0) {
                        if (action->target_id != target_id) {
                            continue;
                        }
                    } else if (!(behavior->flags_1 & AI_ABILITY_TARGET_ALLIES)
                        && ai->unit_acts_before_me[target_id] != 0) {
                        continue;
                    }
                }
                battle_action_finalize_attack_and_flag_reactions(target_id);
                if (ai->hit_counter == 0xff) {
                    continue;
                }
                if (((behavior->flags_1 & AI_ABILITY_TARGET_ENEMIES) && ai->unit_decisions[target_id].enemy_flag != 0)
                    || ((behavior->flags_1 & AI_ABILITY_TARGET_ALLIES)
                        && ai->unit_decisions[target_id].enemy_flag == 0)) {
                    ai->current_action.rank.priority += positive;
                } else if (((behavior->flags_1 & AI_ABILITY_TARGET_ENEMIES)
                               && ai->unit_decisions[target_id].enemy_flag == 0)
                    || ((behavior->flags_1 & AI_ABILITY_TARGET_ALLIES)
                        && ai->unit_decisions[target_id].enemy_flag != 0)) {
                    ai->current_action.rank.priority -= negative;
                }
                ai->hit_counter++;
                ai->total_hit_percent += result->attack_accuracy;
                if (target_id == ai->acting_unit_id) {
                    self_hit = 1;
                }
                if (ai->targetability.live.unit_targetable[target_id] != 0) {
                    ai->valuable_target_hit = 1;
                }
            }
        }
        battle_action_store_target_stats_pointer_data(action->actor_id);
    } while (work.continue_attack != 0);
    if (ai->hit_counter != 0xff && ai->ranked_origin_scan != 0) {
        if (ai->ability_effect_on_self == AI_ABILITY_EFFECT_USEFUL && self_hit != 0) {
            ai->useful_on_caster_flag = 1;
        } else if (ai->ability_effect_on_self == AI_ABILITY_EFFECT_ADVERSE && self_hit == 0) {
            ai->useful_on_caster_flag = 1;
        }
    }
    battle_action_store_acting_unit_data(&g_battle_unit_stats[action->actor_id]);
    if (reaction_mode == 0) {
        /* Reuse the finished loop counter's low 16 bits for the reaction ID. */
        while ((target_id = battle_reaction_prepare_next((s16*)&target_index)) != -1) {
            if (ai->unit_acts_before_me[target_id] == 0) {
                battle_ai_simulate_strikes(&g_battle_unit_stats[target_id].action_command, 1);
            }
        }
    }
}

/*
 * Save baseline unit data, or suspend simulated status and restore baseline status.
 *
 * BASELINE saves selected fields, action records, and team Golem values. Any
 * non-baseline mode saves only the compact status record before restoring the
 * baseline identity, death counter, current status, and Death Sentence CT.
 */
static void battle_ai_save_unit_state(ai_snapshot_mode_e mode) {
    ai_data_t* ai = &g_ai_data_base;
    ai_status_snapshot_t* status;
    ai_extended_snapshot_t* snapshot;
    unit_stats_t* unit;
    s32 i;
    s32 id;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        id = ai->unit_compact_indices[i];
        if (id == 0xff) {
            continue;
        }
        unit = &g_battle_unit_stats[i];
        if (mode != AI_SNAPSHOT_BASELINE) {
            status = &ai->unit_status_records[id];
        } else {
            status = &g_ai_unit_snapshots[id].status;
        }
        status->entd_slot = unit->entd_slot;
        status->death_counter = unit->death_counter;
        battle_mem_copy_bytes(status->current_status, unit->status_sets.current, sizeof(status->current_status));
        if (mode != AI_SNAPSHOT_BASELINE) {
            status->death_sentence_ct = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)];
            snapshot = &g_ai_unit_snapshots[id];
            unit->entd_slot = snapshot->status.entd_slot;
            unit->death_counter = snapshot->status.death_counter;
            unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)]
                = snapshot->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)];
            battle_mem_copy_bytes(
                unit->status_sets.current, snapshot->status.current_status, sizeof(snapshot->status.current_status));
        } else {
            snapshot = (ai_extended_snapshot_t*)status;
            snapshot->team_flags = unit->team_flags;
            battle_mem_copy_bytes(snapshot->status_ct, unit->status_ct, BATTLE_TIMED_STATUS_COUNT);
            battle_mem_copy_bytes(snapshot->equipment, unit->equipment, sizeof(snapshot->equipment));
            snapshot->brave = unit->brave;
            battle_mem_copy_bytes(&snapshot->faith, &unit->faith, 4);
            snapshot->mp = unit->mp;
            battle_mem_copy_bytes(snapshot->base_attributes, unit->base_attributes, 3);
            battle_mem_copy_bytes(snapshot->attributes, unit->attributes, 4);
            battle_mem_copy_bytes(&snapshot->x, &unit->x, 3);
            snapshot->charged_ability_ct = unit->charged_ability_ct;
            battle_mem_copy_bytes(&snapshot->has_turn, &unit->has_turn, 3);
            snapshot->mount_info = unit->mount_info;
            battle_mem_copy_bytes(&snapshot->auto_battle_setting, &unit->auto_battle_setting, 8);
            battle_mem_copy_u16_values(&ai->unit_action_records[id], &unit->action_command, 20);
        }
    }
    if (mode == AI_SNAPSHOT_BASELINE) {
        battle_mem_copy_bytes(ai->team_golem, g_battle_team_golem, sizeof(ai->team_golem));
    }
}

/*
 * Restore baseline unit data or suspended simulation status.
 *
 * BASELINE restores selected baseline fields, action records, and team Golem
 * values. Any non-baseline mode restores only the compact status record.
 */
static void battle_ai_restore_unit_state(ai_snapshot_mode_e mode) {
    ai_data_t* ai = &g_ai_data_base;
    ai_status_snapshot_t* status;
    ai_extended_snapshot_t* snapshot;
    unit_stats_t* unit;
    s32 i;
    s32 id;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        id = ai->unit_compact_indices[i];
        if (id == 0xff) {
            continue;
        }
        unit = &g_battle_unit_stats[i];
        if (mode != AI_SNAPSHOT_BASELINE) {
            status = &ai->unit_status_records[id];
        } else {
            status = &g_ai_unit_snapshots[id].status;
        }
        unit->entd_slot = status->entd_slot;
        unit->death_counter = status->death_counter;
        battle_mem_copy_bytes(unit->status_sets.current, status->current_status, sizeof(status->current_status));
        if (mode != AI_SNAPSHOT_BASELINE) {
            unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] = status->death_sentence_ct;
        } else {
            snapshot = (ai_extended_snapshot_t*)status;
            unit->team_flags = snapshot->team_flags;
            battle_mem_copy_bytes(unit->status_ct, snapshot->status_ct, BATTLE_TIMED_STATUS_COUNT);
            battle_mem_copy_bytes(unit->equipment, snapshot->equipment, sizeof(snapshot->equipment));
            unit->brave = snapshot->brave;
            battle_mem_copy_bytes(&unit->faith, &snapshot->faith, 4);
            unit->mp = snapshot->mp;
            battle_mem_copy_bytes(unit->base_attributes, snapshot->base_attributes, 3);
            battle_mem_copy_bytes(unit->attributes, snapshot->attributes, 4);
            battle_mem_copy_bytes(&unit->x, &snapshot->x, 3);
            unit->charged_ability_ct = snapshot->charged_ability_ct;
            battle_mem_copy_bytes(&unit->has_turn, &snapshot->has_turn, 3);
            unit->mount_info = snapshot->mount_info;
            battle_mem_copy_bytes(&unit->auto_battle_setting, &snapshot->auto_battle_setting, 8);
            battle_mem_copy_u16_values(&unit->action_command, &ai->unit_action_records[id], 20);
        }
    }
    if (mode == AI_SNAPSHOT_BASELINE) {
        battle_mem_copy_bytes(g_battle_team_golem, ai->team_golem, sizeof(ai->team_golem));
    }
}

/* Keep in the target list only the units the considered ability is useful on.
 * Returns 1 while one is left. */
static s32 battle_ai_filter_useful_targets(void) {
    ai_data_t* ai;
    s32 result;
    s32 i;

    result = 0;
    ai = &g_ai_data_base;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (ai->targetability.live.unit_targetable[i] != 0) {
            if (battle_ai_classify_ability_effect(i, 1) == AI_ABILITY_EFFECT_USEFUL) {
                result = 1;
            } else {
                ai->targetability.live.unit_targetable[i] = 0;
            }
        }
    }
    return result;
}

/*
 * Return the AI's usefulness category for one target.
 *
 * This is the acting side's heuristic, not the sign of HP damage or a success
 * guarantee. Nonzero check_reflect enables the timing-aware Reflect restriction.
 */
ai_ability_effect_e battle_ai_classify_ability_effect(s32 unit_id, s32 check_reflect) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];
    ai_unit_decision_t* decision = &ai->unit_decisions[unit_id];
    s32 ct = g_ai_data_base.considered_ability.ct;
    s32 i;
    s32 bit;
    s32 mask;
    s32 status;
    s16 ability_id;

    if ((g_ai_data_base.considered_ability.ai_flags.word & AI_ABILITY_ENEMIES_ONLY) && !decision->enemy_flag)
        return AI_ABILITY_EFFECT_NONE;
    if ((ai->considered_ability.ai_flags.word & AI_ABILITY_ALLIES_ONLY) && decision->enemy_flag)
        return AI_ABILITY_EFFECT_NONE;
    if (unit_id == ai->acting_unit_id) {
        ability_id = ai->considered_ability.ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST
            && (g_main_ability_range_data[ability_id].flags_2 & ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER))
            return AI_ABILITY_EFFECT_NONE;
    }
    if (check_reflect && (ai->considered_ability.ai_flags.word & AI_ABILITY_REFLECTABLE)
        && battle_ai_is_status_active_through_delay(ct, unit, STATUS_ID_REFLECT))
        return AI_ABILITY_EFFECT_NONE;
    if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTED_BY_FAITH)
        && battle_ai_is_status_active_through_delay(ct, unit, STATUS_ID_INNOCENT))
        return AI_ABILITY_EFFECT_NONE;
    if (battle_ai_is_status_active_through_delay(ct, unit, STATUS_ID_JUMP))
        return AI_ABILITY_EFFECT_NONE;

    /* Death/revival timing determines which effects can still matter. */
    i = 0;
    if ((ai->considered_ability.ai_flags.word & AI_ABILITY_CANCEL_STATUS)
        && (ai->considered_ability.parameters.fields.status_infliction[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
            & STATUS_BYTE_MASK(STATUS_ID_DEAD)))
        i = 1;
    if (decision->targeting_flags_2 & AI_TARGET_DEAD_WITH_RERAISE) {
        s32 ability_ct = ai->considered_ability.ct;
        if (ability_ct < battle_ai_calculate_clockticks_until_unit_acts(unit))
            return AI_ABILITY_EFFECT_NONE;
        if (i)
            return AI_ABILITY_EFFECT_NONE;
    }
    if (decision->targeting_flags_2 & AI_TARGET_DEAD_WITHOUT_RERAISE) {
        s32 ability_ct = ai->considered_ability.ct;
        if (battle_ai_calculate_clockticks_until_death_counter_expires(unit) < ability_ct)
            return AI_ABILITY_EFFECT_NONE;
        if (!i)
            return AI_ABILITY_EFFECT_NONE;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
        && (!(ai->considered_ability.ai_flags.word & AI_ABILITY_CANCEL_STATUS)
            || !(ai->considered_ability.parameters.fields.status_infliction[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
                & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))))
        return AI_ABILITY_EFFECT_NONE;
    if (battle_ai_is_status_active_through_delay(ct, unit, STATUS_ID_WALL)
        && (!(ai->considered_ability.ai_flags.word & AI_ABILITY_CANCEL_STATUS)
            || !(ai->considered_ability.parameters.fields.status_infliction[STATUS_BYTE_INDEX(STATUS_ID_WALL)]
                & STATUS_BYTE_MASK(STATUS_ID_WALL))))
        return AI_ABILITY_EFFECT_NONE;

    i = ai->considered_ability.element;
    if (i) {
        if (i == (i & unit->elemental_affinity[ELEMENTAL_AFFINITY_NULLIFY])
            || (i == 8
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
                    & STATUS_BYTE_MASK(STATUS_ID_FLOAT)))) {
            return AI_ABILITY_EFFECT_NONE;
        }
    }

    if (ai->considered_ability.ai_flags.word & AI_ABILITY_TARGET_ENEMIES) {
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_UNDEAD_REVERSE) {
            if (decision->enemy_flag) {
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))
                    return AI_ABILITY_EFFECT_ADVERSE;
            } else if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
                           & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))) {
                return AI_ABILITY_EFFECT_ADVERSE;
            }
        } else if (decision->enemy_flag) {
            if (unit->elemental_affinity[ELEMENTAL_AFFINITY_ABSORB] & ai->considered_ability.element)
                return AI_ABILITY_EFFECT_ADVERSE;
        } else {
            if (battle_ai_is_status_active_through_delay(ct, unit, STATUS_ID_CHARM)
                && battle_ai_can_status_cancellation_affect_turn(unit, STATUS_ID_CHARM))
                i = 1;
            else
                i = 0;
            if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_HP)
                && g_main_action_menu_types_by_skillset[ai->considered_ability.skillset] != ACTION_MENU_TYPE_CHARGE) {
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
                    & STATUS_BYTE_MASK(STATUS_ID_CONFUSION))
                    return AI_ABILITY_EFFECT_USEFUL;
                if (battle_ai_is_status_active_through_delay(ct, unit, STATUS_ID_SLEEP))
                    return AI_ABILITY_EFFECT_USEFUL;
                if (i)
                    return AI_ABILITY_EFFECT_USEFUL;
            }
            if ((unit->elemental_affinity[ELEMENTAL_AFFINITY_ABSORB] & ai->considered_ability.element)
                && (decision->targeting_flags_2 & AI_TARGET_HP_BELOW_HALF))
                return AI_ABILITY_EFFECT_USEFUL;
            if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
                    & (STATUS_BYTE_MASK(STATUS_ID_CONFUSION) | STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)))
                || i) {
                if (ai->considered_ability.ai_flags.word & AI_ABILITY_ADD_STATUS) {
                    /* Early acceptance of a disabling effect bypasses the generic
                     * already-present/immunity filters below. Status bytes 1..4
                     * are loaded together as an aligned 32-bit value by the target. */
                    s32 status_mask = ai->considered_ability.parameters.packed.status_tail
                        & (AI_STATUS_TAIL_PACKED_MASK(STATUS_ID_SLOW) | AI_STATUS_TAIL_PACKED_MASK(STATUS_ID_STOP)
                            | AI_STATUS_TAIL_PACKED_MASK(STATUS_ID_SLEEP)
                            | AI_STATUS_TAIL_PACKED_MASK(STATUS_ID_DONT_MOVE)
                            | AI_STATUS_TAIL_PACKED_MASK(STATUS_ID_DONT_ACT));
                    if (status_mask)
                        return AI_ABILITY_EFFECT_USEFUL;
                }
            }
            return AI_ABILITY_EFFECT_ADVERSE;
        }
        if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_HP)
            && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
                & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE)))
            return AI_ABILITY_EFFECT_USEFUL;
        if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_MP) && unit->mp > decision->lowest_mp_cost)
            return AI_ABILITY_EFFECT_USEFUL;
    }

    if (ai->considered_ability.ai_flags.word & AI_ABILITY_TARGET_ALLIES) {
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_UNDEAD_REVERSE) {
            if (decision->enemy_flag) {
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))
                    return AI_ABILITY_EFFECT_USEFUL;
                return AI_ABILITY_EFFECT_ADVERSE;
            }
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))
                return AI_ABILITY_EFFECT_ADVERSE;
        } else if (decision->enemy_flag)
            return AI_ABILITY_EFFECT_ADVERSE;
        /* Death Sentence: the Add Reraise shortcut likewise bypasses the
         * generic already-present/immunity filters. */
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
            & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE)) {
            if ((ai->considered_ability.ai_flags.word & AI_ABILITY_ADD_STATUS)
                && (ai->considered_ability.parameters.fields.status_infliction[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)]
                    & STATUS_BYTE_MASK(STATUS_ID_RERAISE)))
                return AI_ABILITY_EFFECT_USEFUL;
            return AI_ABILITY_EFFECT_NONE;
        }
        if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_HP)
            && (decision->targeting_flags_2 & AI_TARGET_HP_BELOW_HALF))
            return AI_ABILITY_EFFECT_USEFUL;
        if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_MP)
            && (decision->targeting_flags_2 & AI_TARGET_MP_CONSTRAINED))
            return AI_ABILITY_EFFECT_USEFUL;
    }

    if (!(ai->considered_ability.ai_flags.word & (AI_ABILITY_TARGET_ALLIES | AI_ABILITY_TARGET_ENEMIES))) {
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_HP)
            return AI_ABILITY_EFFECT_USEFUL;
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_MP)
            return AI_ABILITY_EFFECT_USEFUL;
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_ADD_STATUS) {
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            mask = ai->considered_ability.parameters.fields.status_infliction[i];
            mask ^= mask & unit->status_sets.current[i];
            if (mask) {
                mask ^= mask & unit->status_sets.immunity[i];
                if (mask) {
                    switch (i) {
                    case 1:
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_SILENCE)) && !decision->silence_mod)
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_SILENCE);
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_DARKNESS)) && decision->evade_mod < 2)
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_DARKNESS);
                        break;
                    case 2:
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_RERAISE))
                            && !(decision->targeting_flags_2 & AI_TARGET_HP_BELOW_HALF))
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_RERAISE);
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_FROG)) && !decision->enemy_flag)
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_FROG);
                        if (!(decision->targeting_flags_2 & AI_TARGET_HP_BELOW_THREE_QUARTERS))
                            mask &= (u8)~STATUS_BYTE_MASK(STATUS_ID_FLOAT);
                        break;
                    case 3:
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_REGEN))
                            && !(decision->targeting_flags_2 & AI_TARGET_HP_BELOW_HALF))
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_REGEN);
                        if (!(decision->targeting_flags_2 & AI_TARGET_HP_BELOW_THREE_QUARTERS))
                            mask &= (u8) ~(STATUS_BYTE_MASK(STATUS_ID_PROTECT) | STATUS_BYTE_MASK(STATUS_ID_SHELL)
                                | STATUS_BYTE_MASK(STATUS_ID_WALL));
                        if ((mask & (STATUS_BYTE_MASK(STATUS_ID_HASTE) | STATUS_BYTE_MASK(STATUS_ID_SLOW)))
                            && ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_STOP)]
                                    & STATUS_BYTE_MASK(STATUS_ID_STOP))
                                || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)]
                                    & STATUS_BYTE_MASK(STATUS_ID_SLEEP))))
                            mask &= (u8) ~(STATUS_BYTE_MASK(STATUS_ID_HASTE) | STATUS_BYTE_MASK(STATUS_ID_SLOW));
                        break;
                    case 4:
                        if (!(decision->targeting_flags_2 & AI_TARGET_HP_BELOW_THREE_QUARTERS))
                            mask &= (u8) ~(STATUS_BYTE_MASK(STATUS_ID_FAITH) | STATUS_BYTE_MASK(STATUS_ID_REFLECT));
                        break;
                    }
                    if (mask)
                        return AI_ABILITY_EFFECT_USEFUL;
                }
            }
        }
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_CANCEL_STATUS) {
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            mask = ai->considered_ability.parameters.fields.status_infliction[i];
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
                                    || !battle_ai_can_status_cancellation_affect_turn(unit, status))
                                    mask ^= flag;
                            }
                        }
                    }
                    switch (i) {
                    case 1:
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_SILENCE)) && !decision->silence_mod)
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_SILENCE);
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_DARKNESS)) && !decision->evade_mod)
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_DARKNESS);
                        break;
                    case 2:
                        if ((mask & STATUS_BYTE_MASK(STATUS_ID_FROG)) && decision->enemy_flag)
                            mask ^= STATUS_BYTE_MASK(STATUS_ID_FROG);
                        break;
                    }
                    if (mask)
                        return AI_ABILITY_EFFECT_USEFUL;
                }
            }
        }
    }
    /* Late stat-effect gate: Praise, Accumulate, and Yell only.
     * Odd parity still falls through to the Unequip flag below. */
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_STATS) {
        ability_id = ai->considered_ability.ability_id;
        if (ability_id != ABILITY_ID_TALK_PRAISE && ability_id != ABILITY_ID_BASIC_SKILL_ACCUMULATE
            && ability_id != ABILITY_ID_GUTS_YELL)
            return AI_ABILITY_EFFECT_USEFUL;
        if (!(rand() & 1))
            return AI_ABILITY_EFFECT_USEFUL;
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_UNEQUIP)
        return AI_ABILITY_EFFECT_USEFUL;
    return AI_ABILITY_EFFECT_NONE;
}

/*
 * Test whether a current status lasts through the requested charge time.
 *
 * Equality counts as present. Jump uses its charged-ability timer; other
 * timed statuses use their duration unless the status is innate.
 */
s32 battle_ai_is_status_active_through_delay(s32 delay, const unit_stats_t* unit, s32 status_id) {
    s32 byte_off;

    byte_off = status_id / 8;

    if (!(unit->status_sets.current[byte_off] & (0x80 >> (status_id - byte_off * 8)))) {
        return 0;
    }
    if (status_id == STATUS_ID_JUMP) {
        if (unit->charged_ability_ct < delay) {
            return 0;
        }
    } else {
        if (status_id < STATUS_ID_POISON) {
            return 1;
        }
        if (unit->status_sets.innate[byte_off] & (0x80 >> (status_id - byte_off * 8))) {
            return 1;
        }
        if (unit->status_ct[STATUS_CT_INDEX(status_id)] < delay) {
            return 0;
        }
    }
    return 1;
}

/*
 * Reject removal of a control status that expires before the unit's next turn.
 *
 * Only Charm, Don't Move, and Don't Act are filtered; other IDs return true.
 * Equality counts as lasting through the turn. This does not test presence.
 */
static s32 battle_ai_can_status_cancellation_affect_turn(const unit_stats_t* unit, s32 status) {
    if (status == STATUS_ID_CHARM || (status - STATUS_ID_DONT_MOVE) < 2U) {
        if (battle_ai_calculate_clockticks_until_unit_acts(unit) > unit->status_ct[STATUS_CT_INDEX(status)]) {
            return 0;
        }
    }
    return 1;
}

/* For an enemy-targeting ability, drop from the target list each enemy that
 * will act before the ability lands. Returns 1 while a target is left. */
static s32 battle_ai_exclude_enemies_acting_before_ct(void) {
    s32 result;
    s32 i;
    ai_data_t* ai;
    ai_data_t* walk;

    result = 0;
    ai = &g_ai_data_base;
    /* The target walks a pointer that starts at the block base and steps by
     * sizeof(ai_unit_decision_t), reaching unit_decisions[i].enemy_flag as a
     * 0x1834 displacement; a pointer started at unit_decisions adds an addiu. */
    for (i = 0, walk = ai; i < BATTLE_UNIT_SLOT_COUNT;
        i++, walk = (ai_data_t*)((u8*)walk + sizeof(ai_unit_decision_t))) {
        if (ai->targetability.live.unit_targetable[i] != 0) {
            if ((ai->considered_ability.ai_flags.word & AI_ABILITY_FLAG_1_TARGET_ENEMIES)
                && (walk->unit_decisions[0].enemy_flag != 0) && (battle_ai_can_unit_act_within_charge_time(i) != 0)) {
                ai->targetability.live.unit_targetable[i] = 0;
            } else {
                result = 1;
            }
        }
    }
    return result;
}

/* 1 when the unit gets a turn by the considered ability's charge deadline
 * and can act by then; 0 for an instant ability. */
static s32 battle_ai_can_unit_act_within_charge_time(s32 unit_index) {
    s32 min_ct;
    u8 ct;

    ct = g_ai_data_base.considered_ability.ct;
    if (ct != 0) {
        min_ct = battle_ai_calculate_clockticks_until_unit_acts(&g_battle_unit_stats[unit_index]);
        if (min_ct != 0x7FFFFFFF) {
            if (ct >= min_ct) {
                return battle_ai_is_unit_active_after_delay(ct, unit_index);
            }
        }
    }
    return 0;
}

/* 1 when the unit can act limit clockticks from now: present, Speed above 0,
 * not crystal, petrified or treasure, with no Stop, Don't Move or Sleep that
 * lasts that long, and alive, or dead with Reraise and due a turn by then. */
static s32 battle_ai_is_unit_active_after_delay(s32 limit, s32 unit_id) {
    unit_stats_t* unit = &g_battle_unit_stats[unit_id];

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 0;
    }
    if (unit->attributes[UNIT_ATTRIBUTE_SPEED] == 0) {
        return 0;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return 0;
    }
    if (battle_ai_is_status_active_through_delay(limit, unit, STATUS_ID_STOP) != 0) {
        return 0;
    }
    if (battle_ai_is_status_active_through_delay(limit, unit, STATUS_ID_DONT_MOVE) != 0) {
        return 0;
    }
    if (battle_ai_is_status_active_through_delay(limit, unit, STATUS_ID_SLEEP) != 0) {
        return 0;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) == 0) {
        return 1;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)] & STATUS_BYTE_MASK(STATUS_ID_RERAISE)) == 0
        || limit < battle_ai_calculate_clockticks_until_unit_acts(unit)) {
        return 0;
    }
    return 1;
}

/* Position the temporary simulation unit at the supplied AI coordinates. */
void battle_ai_move_temp_unit_to_coords(const ai_coords_t* coord) {
    g_ai_data_base.acting_unit->x = coord->fields.x;
    g_ai_data_base.acting_unit->position.y = coord->fields.y;
    g_ai_data_base.acting_unit->position.higher_elevation = coord->fields.elevation;
}

/* Pack a unit's tile as AI coordinates: x, y and the elevation bit (position
 * bit 15). */
void battle_ai_copy_unit_coordinates(s32 unit_idx, ai_coords_t* dst) {
    dst->fields.x = g_battle_unit_stats[unit_idx].x;
    dst->fields.y = g_battle_unit_stats[unit_idx].position.y;
    dst->fields.elevation = g_battle_unit_stats[unit_idx].position.higher_elevation;
    dst->fields.zero = 0;
}
