/* AI cast origins and aim tiles, target evaluation and action simulation. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/ai.h"
#include "fft/ai_decision.h"
#include "fft/battle_map.h"
#include "fft/command.h"
#include "fft/projectile.h"
#include "fft/reaction.h"
#include "fft/skillset.h"
#include "fft/status.h"
#include "fft/target.h"
#include "fft/turn.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/stddef.h"
#include "psx/types.h"

typedef enum ai_tile_mask_mode {
    AI_TILE_MASK_RANGE = 0,
    AI_TILE_MASK_AOE = 1,
    AI_TILE_MASK_RANGE_PLUS_AOE = 2,
    AI_TILE_MASK_EXPLICIT = 3
} ai_tile_mask_mode_e;

/* Baseline records include action/equipment data; simulated status uses the compact records. */
typedef enum ai_snapshot_mode { AI_SNAPSHOT_BASELINE = 0, AI_SNAPSHOT_SIMULATED_STATUS = 1 } ai_snapshot_mode_e;

/* Only the four cardinal hints index the tile-delta table. */
enum { AI_CARDINAL_DIRECTION_COUNT = 4 };

/* Cardinal map-tile deltas stored as wrapping unsigned bytes. */
typedef struct ai_facing_tile_offsets {
    u8 by_direction[AI_CARDINAL_DIRECTION_COUNT][2];
} ai_facing_tile_offsets_t;

/* Origin masks, Math Skill iteration and facing offsets. */
extern s32 g_ai_origin_self_target_stage;
extern s32 g_ai_math_ability_index;
extern u16* g_ai_math_ability_list; /* borrowed from main's temporary skillset list */
extern s32 g_ai_math_skillset_id;
extern s32 g_ai_math_skillset_index;

/* Diamond rows centered on map column 7, for radii 0 through 7.
 * The unchecked reader also reaches the following status weights for radii 8..15. */
static u16 g_ai_origin_range_row_masks[] = { 0x0100, 0x0380, 0x07c0, 0x0fe0, 0x1ff0, 0x3ff8, 0x7ffc, 0xfffe };

/* Signed scoring weights, indexed by status ID. */
static s16 g_ai_status_priority_weights[STATUS_COUNT]
    = { 0, -192, -192, -39, 0, 0, 0, 0, -116, -231, -64, -64, -90, -116, 0, -192, -7, 12, 51, 38, -39, -26, -52, -32,
          -26, 25, 25, 25, 19, -39, -90, 64, 6, -7, -64, -39, -39, -64, 25, -103 }; /* 0x8019f308..0x8019f357 */

static ai_facing_tile_offsets_t g_ai_facing_tile_offsets = { { { 0, 0xff }, { 0xff, 0 }, { 0, 1 }, { 1, 0 } } };

static void ai_origin_build_monster_skill_mask(void);
static void ai_origin_build_targetable_mask(ai_tile_mask_mode_e mode, s32 radius);
static s32 ai_status_can_cancellation_affect_turn(const unit_stats_t* unit, s32 status);
static s32 ai_can_unit_move_by_charge_deadline(s32 unit_index);
static s32 ai_origin_can_use_elemental(const ai_coords_t* coords);
static void ai_rank_clear_current_decision(void);
static s32 ai_math_evaluate_targets(void);
static s32 ai_origin_evaluate_reachable(void);
static s32 ai_origin_evaluate_reflected(void);
static s32 ai_origin_evaluate_self_target(void);
static s32 ai_origin_evaluate_aim_tiles(void);
static s32 ai_target_exclude_enemies_with_move_opportunity(void);
static s32 ai_target_filter_useful(void);
static void ai_origin_finalize_destination(void);
static s32 ai_target_find_unit_on_panel(void);
static s32 ai_facing_is_tile_blocked(const ai_coords_t* source, const ai_coords_t* target);
static s32 ai_is_unit_movement_eligible_after_delay(s32 limit, s32 unit_id);
static void ai_simulate_restore_unit_state(ai_snapshot_mode_e mode);
static void ai_simulate_save_unit_state(ai_snapshot_mode_e mode);
static s32 ai_simulate_action_hits(action_command_t* action);
static void ai_simulate_movement_and_pickup(void);
static void ai_simulate_strikes(action_command_t* action, s32 reaction_mode);
static void ai_origin_take_next(void);

/* Copy byte_count bytes as 16-bit values (an odd last byte is dropped); the AI's
 * copier for its tile maps and action records. */
void ai_mem_copy_u16_values(void* destination, const void* source, s32 byte_count) {
    u16* dst = destination;
    const u16* src = source;
    s32 count = byte_count / 2;
    s32 i;
    for (i = 0; i < count; i++) {
        *dst++ = *src++;
    }
}

/* Copy len bytes from source to destination; nothing when len is 0 or less. */
void ai_mem_copy_bytes(void* destination, const void* source, s32 len) {
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
void ai_mem_zero_32bit_values(void* destination, s32 byte_count) {
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
 * A hint other than AI_DIRECTION_CHOOSE_AT_WAIT is returned as is. Otherwise
 * face the nearest imminent enemy whose direction is viable, trying each in
 * turn, then the nearest enemy of any kind; a direction is viable when its
 * adjacent tile is on the map, unblocked and not 3 or more higher (all four
 * are when none is). Failing that, keep the current facing if viable, else
 * the last viable direction. The original does not bounds-check the direction
 * lookup, so coincident source and target coordinates can read one byte past
 * the four-byte viability array.
 */
s32 ai_facing_choose_for_wait(void) {
    ai_data_t* ai = &g_ai_data;
    ai_wait_facing_work_t work;
    s32 direction;
    s32 target_id;
    s32 acting_unit_id;
    s32 fallback_direction;
    s32 tried_any_enemy;
    s32 facing;

    if (ai->selected_action.wait_facing_hint != AI_DIRECTION_CHOOSE_AT_WAIT)
        return ai->selected_action.wait_facing_hint;

    ai_copy_unit_coordinates(ai->acting_unit_id, &ai->acting_unit_coords);
    for (direction = 0; direction < AI_CARDINAL_DIRECTION_COUNT; direction++) {
        work.target_coords.fields.x
            = ai->acting_unit_coords.fields.x + g_ai_facing_tile_offsets.by_direction[direction][0];
        work.target_coords.fields.y
            = ai->acting_unit_coords.fields.y + g_ai_facing_tile_offsets.by_direction[direction][1];
        work.target_coords.fields.layer = 0;
        if (ai_facing_is_tile_blocked(&ai->acting_unit_coords, &work.target_coords) != 0) {
            work.viability.by_direction[direction] = 0;
        } else {
            fallback_direction = direction;
            work.viability.by_direction[direction] = 1;
        }
    }
    if (work.viability.word == 0)
        work.viability.word = 0x01010101;

    for (target_id = 0; target_id < UNIT_SLOT_COUNT; target_id++) {
        ai->targetability.live.unit_targetable[target_id] = 1;
    }
    tried_any_enemy = 0;
    for (;;) {
        acting_unit_id = ai->acting_unit_id;
        target_id = ai_target_find_nearest(AI_NEAREST_IMMINENT_ENEMY);
        if (acting_unit_id == target_id) {
            target_id = ai_target_find_nearest(AI_NEAREST_ENEMY);
            tried_any_enemy = 1;
        }
        ai_copy_unit_coordinates(target_id, &work.target_coords);
        direction = ai_facing_find_target_direction(&work.target_coords, &ai->acting_unit_coords);
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
 * Return 1 for rejection and 0 otherwise. A blocked tile is also rejected.
 * Only the destination's lower layer is checked; a rise of 3 is rejected,
 * but descents are unrestricted. Slope and depth do not affect this test.
 */
static s32 ai_facing_is_tile_blocked(const ai_coords_t* source, const ai_coords_t* target) {
    u8 x = target->fields.x;
    u8 width = g_ai_data.terrain_width;
    u8 y;
    battle_map_tile_t* tile;

    if (x >= width) {
        return 1;
    }
    y = target->fields.y;
    if (y >= g_ai_data.terrain_depth || (tile = &g_battle_map_tile_data[y * width + x])->flags.blocked) {
        return 1;
    }
    return tile->height
        - g_battle_map_tile_data[(source->fields.layer * BATTLE_MAP_TILES_PER_LAYER) + source->fields.y * width
            + source->fields.x]
              .height
        >= 3;
}

/* Reset the selected action's ranking so it is no longer decided, which makes
 * the AI decide afresh on its next command. Called when a turn opens without a
 * scenario event and when a move is interrupted (failed teleport, crystal or
 * treasure pickup, triggered trap). */
void ai_init_selected_action(void) {
    ai_rank_reset_action(&g_ai_data.selected_action);
}

/*
 * Remember a triggered trap unless its tile is already in the cache.
 *
 * Current coordinates and selected-action defaults are updated even when the
 * list is full. Equality includes all four bytes; x == 0xff marks an empty slot.
 * The post-move trap path passes x/layer/y; this routine stores x/y/layer.
 */
void ai_record_triggered_trap_coords(s32 x, s32 y, s32 level) {
    ai_data_t* ai = &g_ai_data;
    s32 i;

    g_ai_data.considered_unit_coords.fields.x = x;
    g_ai_data.considered_unit_coords.fields.y = y;
    g_ai_data.considered_unit_coords.fields.layer = level;
    g_ai_data.considered_unit_coords.fields.zero = 0;
    ai_init_selected_action();
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
s32 ai_is_pathfinding_budget_exhausted(void) {
    return VSync(1) >= AI_PATHFIND_SCANLINE_LIMIT;
}

/*
 * Evaluate target alternatives for the considered ability.
 *
 * Return -1 to suspend and 0 when finished, including rejected abilities.
 * Suspended target searches resume without reinitializing the candidate.
 *
 * An ability the AI may not use, or a random-use ability that loses its roll,
 * is skipped. A map-tile ability is tried from the unit's tile and the
 * scenario's candidate tile (Arithmeticks from the unit's tile, over its
 * calculator combinations). Otherwise the searches run in turn: for a
 * reflectable ability, casts reflected off a Reflect unit; then, on the units
 * the ability would be useful on, casts on the caster itself, unit-targeted
 * casts for an ability that follows its target (split by whether each target
 * acts before the ability lands) and, unless that ability has no area,
 * tile-targeted casts without the enemies that act first. The best cast so far
 * is added to the ranked list as stages finish.
 */
s32 ai_ability_evaluate_outcome(void) {
    ai_data_t* ai = &g_ai_data;
    s32 i;

    if (g_ai_data.resume_pending) {
        /* Resume: each state re-enters the evaluation step that suspended. */
        switch (g_ai_data.outcome_evaluation_state) {
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

    /* Out of scanline budget: suspend; the PREPARE state resumes at this test. */
    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_PREPARE;
        return -1;
    }
    {
        s32 flags = ai->considered_ability.ai_flags.word;
        ai->resume_pending = 0;
        /* The sign bit is AI_ABILITY_USABLE_BY_AI. A random-use ability goes on only when
         * a roll of 0..127 does not exceed random_use_threshold. */
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
    ai->current_action.command.item_id = ai->considered_ability.parameters.fields.item_id;
    for (i = 0; i < UNIT_SLOT_COUNT; i++)
        ai->targetability.live.unit_targetable[i] = ai->targetability.live.unit_active[i];

    if (ai->considered_ability.ai_flags.word & AI_ABILITY_TARGET_MAP_TILES) {
        s32 acting_id = ai->acting_unit_id;
        ai->current_action.command.targeting_type = TARGET_TYPE_UNIT;
        ai->current_action.command.target_id = acting_id;
        ai->current_action.coords = ai->candidate_coords[ai->movement_scenario];
        ai_update_tile_pickup();
        if (g_command_menu_types_by_skillset[ai->considered_ability.skillset] == ACTION_MENU_TYPE_ARITHMETICKS) {
        evaluate_math_targets:
            if (ai_math_evaluate_targets() == -1) {
                ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_MATH_TARGETS;
                return -1;
            }
            return 0;
        }
        if (!ai_target_filter_useful())
            return 0;
    evaluate_map_targets:
        if (ai_origin_evaluate_before_and_after_move() == -1) {
            ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_MAP_TARGETS;
            return -1;
        }
        ai_rank_insert_best();
        return 0;
    }

    if (ai->considered_ability.ai_flags.word & AI_ABILITY_REFLECTABLE) {
    evaluate_reflected_targets:
        if (ai_origin_evaluate_reflected() == -1) {
            ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_REFLECTED_TARGETS;
            return -1;
        }
    }

    if (!ai_target_filter_useful())
        return 0;
    if (ai->targetability.live.unit_targetable[ai->acting_unit_id]) {
    evaluate_self_target_range:
        if (ai_origin_evaluate_self_target() == -1) {
            ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_SELF_TARGET_RANGE;
            return -1;
        }
    }

    ai->ability_effect_on_self = ai_target_classify_ability_effect(ai->acting_unit_id, 1);
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_FOLLOW_TARGET) {
        ai->current_action.command.targeting_type = TARGET_TYPE_UNIT;
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_CONSUME_EVALUATED_AIM_TILES) {
            ai->targetability.live.target_setting_flags |= AI_TARGET_SETTING_CONSUME_EVALUATED_AIM_TILES;
        evaluate_single_unit_pass:
            if (ai_origin_evaluate_reachable() == -1) {
                ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_UNIT_TARGETS;
                return -1;
            }
        } else {
            /* Evaluate complementary CT-filtered target groups, then restore the
             * original eligibility list. The overlapping flag byte stays intact. */
            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                ai->targetability.snapshot.unit_targetable_saved[i] = ai->targetability.live.unit_targetable[i];
                if (ai->targetability.live.unit_targetable[i] && !ai_can_unit_move_by_charge_deadline(i))
                    ai->targetability.live.unit_targetable[i] = 0;
            }
            ai->targetability.live.target_setting_flags |= AI_TARGET_SETTING_CONSUME_EVALUATED_AIM_TILES;
        evaluate_first_ct_group:
            if (ai_origin_evaluate_reachable() == -1) {
                ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_FIRST_CT_GROUP;
                return -1;
            }
            for (i = 0; i < UNIT_SLOT_COUNT; i++)
                if (ai->targetability.snapshot.unit_targetable_saved[i]) {
                    if (ai_can_unit_move_by_charge_deadline(i))
                        ai->targetability.live.unit_targetable[i] = 0;
                    else
                        ai->targetability.live.unit_targetable[i] = 1;
                }
            ai->targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_CONSUME_EVALUATED_AIM_TILES;
        evaluate_second_ct_group:
            if (ai_origin_evaluate_reachable() == -1) {
                ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_SECOND_CT_GROUP;
                return -1;
            }
            for (i = 0; i < UNIT_SLOT_COUNT; i++)
                ai->targetability.live.unit_targetable[i] = ai->targetability.snapshot.unit_targetable_saved[i];
        }

        if (!ai->considered_ability.parameters.fields.aoe) {
            ai_rank_insert_best();
            return 0;
        }
    }

    if (ai_target_exclude_enemies_with_move_opportunity()) {
        ai->current_action.command.targeting_type = TARGET_TYPE_TILE;
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_CONSUME_EVALUATED_AIM_TILES)
            ai->targetability.live.target_setting_flags |= AI_TARGET_SETTING_CONSUME_EVALUATED_AIM_TILES;
    evaluate_tile_targets:
        if (ai_origin_evaluate_reachable() == -1) {
            ai->outcome_evaluation_state = AI_ABILITY_EVALUATION_TILE_TARGETS;
            return -1;
        }
    }

    ai_rank_insert_best();
    return 0;
}

/*
 * Evaluate self-target alternatives from the actor and candidate coordinates.
 *
 * Record the resume stage when returning -1 to suspend. Completed searches
 * restore the actor coordinates and submit the action to the ranked list.
 */
static s32 ai_origin_evaluate_self_target(void) {
    ai_data_t* ai = &g_ai_data;
    ai_tile_mask_mode_e mode;

    if (g_ai_data.resume_pending) {
        /* Resume: re-enter the step that suspended. */
        if (g_ai_origin_self_target_stage == 0)
            goto evaluate_map;
        goto evaluate_targets;
    }
    g_ai_data.targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_CONSUME_EVALUATED_AIM_TILES;
    g_ai_data.current_action.coords = ai->candidate_coords[g_ai_data.movement_scenario];
    ai_update_tile_pickup();
    if ((g_ai_data.considered_ability.ai_flags.word & AI_ABILITY_FOLLOW_TARGET)
        && !(g_ability_range_data[g_ai_data.considered_ability.ability_id].flags_1
            & ABILITY_SECONDARY_FLAG_1_CANNOT_TARGET_SELF)) {
        g_ai_data.current_action.act_before_move = 1;
        g_ai_data.current_action.command.targeting_type = TARGET_TYPE_UNIT;
        g_ai_data.current_action.command.target_id = g_ai_data.acting_unit_id;
    evaluate_map:
        if (ai_origin_evaluate_before_and_after_move() == -1) {
            g_ai_origin_self_target_stage = 0;
            return -1;
        }
    }
    ai->current_action.command.targeting_type = TARGET_TYPE_TILE;
    g_ai_origin_self_target_stage = 0;
    ai->current_action.act_before_move = 1;
    memcpy(&ai->considered_unit_coords, &ai->acting_unit_coords, sizeof(ai_coords_t));
prepare_origin:
    mode = AI_TILE_MASK_AOE;
    if (ai->current_action.command.targeting_type == TARGET_TYPE_UNIT)
        mode = AI_TILE_MASK_EXPLICIT;
    ai_origin_build_targetable_mask(mode, 0);
    ai_mem_copy_u16_values(ai->aim_tiles, ai->targetable_tiles, sizeof(ai->aim_tiles));
    /* Both passes test the unit's current tile, even when it casts after moving
     * (ai_origin_evaluate_before_and_after_move tests the destination). */
    if (ai_origin_is_allowed(&ai->acting_unit_coords)) {
    evaluate_targets:
        if (ai_origin_evaluate_aim_tiles() == -1) {
            /* A first-origin suspension skips the moved-origin pass on resume. */
            g_ai_origin_self_target_stage = 1;
            return -1;
        }
    }
    if (g_ai_origin_self_target_stage == 0) {
        ai->current_action.act_before_move = 0;
        ai->considered_unit_coords = ai->current_action.coords;
        g_ai_origin_self_target_stage++;
        /* A goto retry, not a loop: GCC's loop pass would move the suspend
         * exit out of line. */
        goto prepare_origin;
    }
    ai_place_acting_unit(&ai->acting_unit_coords);
    ai_rank_insert_best();
    return 0;
}

/*
 * Check depth, terrain, and Monster Skill restrictions at an AI origin.
 *
 * Return 1 when these checks permit use, otherwise 0. Geomancy delegates
 * directly to its terrain check; this is not a complete ability-use test.
 * A unit that cannot enter water rejects depth 2 or more, and a Monster Skill
 * ability needs a same-team Monster Skill holder that can react, at most one
 * tile from the origin and within 2 in height.
 */
s32 ai_origin_is_allowed(const ai_coords_t* coords) {
    ai_data_t* ai = &g_ai_data;
    s32 i;
    s32 dx;
    s32 dy;
    s32 height;
    unit_stats_t* unit;

    if (g_ai_data.water_penalty) {
        s32 depth = g_battle_map_tile_data[coords->fields.y * g_ai_data.terrain_width + coords->fields.x
            + (coords->fields.layer * BATTLE_MAP_TILES_PER_LAYER)]
                        .depth_half_height.depth;
        if (depth >= 2)
            return 0;
    }
    if (g_command_menu_types_by_skillset[ai->considered_ability.skillset] == ACTION_MENU_TYPE_ELEMENTS)
        return ai_origin_can_use_elemental(coords);
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_REQUIRES_MONSTER_SKILL) {
        for (i = 0; i < UNIT_SLOT_COUNT; i++) {
            unit = &g_unit_stats[i];
            if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE)
                continue;
            if (!(unit->ability_flags.support[3] & UNIT_SUPPORT_SET_4_MONSTER_SKILL))
                continue;
            if ((unit->initial_team_flags & UNIT_TEAM_MASK) != ai->acting_unit_team)
                continue;
            if (status_unit_has_any_in_set(unit, STATUS_CHECK_SET_PREVENT_REACTION))
                continue;
            dx = unit->x - coords->fields.x;
            if (dx < 0)
                dx = -dx;
            dy = unit->position.y - coords->fields.y;
            if (dy < 0)
                dy = -dy;
            if (dx + dy < 2) {
                height = ai_calculate_height_difference(coords, unit);
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
s32 ai_calculate_height_difference(const ai_coords_t* coords, const unit_stats_t* unit) {
    battle_map_tile_t* tile;
    s32 h1;
    s32 h2;

    tile = &g_battle_map_tile_data[unit->position.higher_elevation * BATTLE_MAP_TILES_PER_LAYER
        + unit->position.y * g_ai_data.terrain_width + unit->x];
    h1 = tile->height * 2 + tile->depth_half_height.half_height;
    tile = &g_battle_map_tile_data[(coords->fields.layer * BATTLE_MAP_TILES_PER_LAYER)
        + coords->fields.y * g_ai_data.terrain_width + coords->fields.x];
    h2 = tile->height * 2 + tile->depth_half_height.half_height;
    return (h1 - h2) / 2;
}

/*
 * Test whether the terrain selects the considered Geomancy ability.
 *
 * Return 1 for the terrain table's matching ability ID, otherwise 0.
 */
static s32 ai_origin_can_use_elemental(const ai_coords_t* coords) {
    u32 tile_id;
    u32 tile_type;
    u32 ability_terrain;

    tile_id = (coords->fields.layer * BATTLE_MAP_TILES_PER_LAYER) + coords->fields.y * g_ai_data.terrain_width
        + coords->fields.x;
    tile_type = g_battle_map_tile_data[tile_id].surface.type;
    ability_terrain = g_terrain_geomancy_abilities[tile_type];
    return ability_terrain == g_ai_data.considered_ability.ability_id;
}

/*
 * Evaluate reachable origins for reflecting an ability onto another unit.
 *
 * For each unit whose Reflect outlasts the charge and each useful target, when
 * neither acts before the ability lands, the origin is the reachable tile
 * mirroring the target through the reflector (2 * reflector - target), from
 * which a cast aimed at the reflector bounces onto the target.
 *
 * Return -1 to suspend and 0 after restoring the temporary unit's position
 * and submitting the action to the ranked list. Stored counters survive
 * suspension, but the target does not reload the local reflector pointer
 * on resume; later target iterations can therefore use the caller's s2.
 */
static s32 ai_origin_evaluate_reflected(void) {
    ai_data_t* ai = &g_ai_data;
    unit_stats_t* reflector;
    ai_unit_decision_t* decision;
    ai_tile_mask_mode_e mode;
    s32 ct;

    if (g_ai_data.resume_pending) {
        /* Resume: the target jumps into the middle of the origin loop. */
        goto evaluate_targets;
    }
    g_ai_data.current_action.reflected_action = 1;
    g_ai_data.targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_CONSUME_EVALUATED_AIM_TILES;
    for (ai->reflector_unit_index = 0; ai->reflector_unit_index < UNIT_SLOT_COUNT; ai->reflector_unit_index++) {
        reflector = &g_unit_stats[ai->reflector_unit_index];
        decision = &ai->unit_decisions[ai->reflector_unit_index];
        if (decision->targeting_flags_2 & AI_TARGET_UNTARGETABLE)
            continue;
        if (ai->reflector_unit_index == ai->acting_unit_id)
            continue;
        if (!ai_status_is_active_through_delay(ai->considered_ability.ct, reflector, STATUS_ID_REFLECT))
            continue;
        if (ai_can_unit_move_by_charge_deadline(ai->reflector_unit_index))
            continue;
        if (decision->targeting_flags_2 & AI_TARGET_DEAD_WITHOUT_RERAISE) {
            ct = ai->considered_ability.ct;
            if (ai_calculate_clockticks_until_death_counter_expires(reflector) < ct)
                continue;
        }
        ai->current_action.command.targeting_type = TARGET_TYPE_TILE;
        ai_copy_unit_coordinates(ai->reflector_unit_index, &ai->considered_unit_coords);
        mode = AI_TILE_MASK_AOE;
        if (ai->current_action.command.targeting_type == TARGET_TYPE_UNIT)
            mode = AI_TILE_MASK_EXPLICIT;
        ai_origin_build_targetable_mask(mode, 0);
        ai_mem_copy_u16_values(ai->aim_tiles, ai->targetable_tiles, sizeof(ai->aim_tiles));
        for (ai->reflected_target_unit_index = 0; ai->reflected_target_unit_index < UNIT_SLOT_COUNT;
            ai->reflected_target_unit_index++) {
            if (!ai->targetability.live.unit_targetable[ai->reflected_target_unit_index])
                continue;
            if (ai->reflected_target_unit_index == ai->reflector_unit_index)
                continue;
            if (ai->reflected_target_unit_index == ai->acting_unit_id)
                continue;
            if (ai_can_unit_move_by_charge_deadline(ai->reflected_target_unit_index))
                continue;
            if (ai_target_classify_ability_effect(ai->reflected_target_unit_index, 0) != AI_ABILITY_EFFECT_USEFUL)
                continue;
            ai->current_action.coords.fields.x = reflector->x * 2 - g_unit_stats[ai->reflected_target_unit_index].x;
            ai->current_action.coords.fields.y
                = reflector->position.y * 2 - g_unit_stats[ai->reflected_target_unit_index].position.y;
            if (ai->current_action.coords.fields.x >= ai->terrain_width)
                continue;
            if (ai->current_action.coords.fields.y >= ai->terrain_depth)
                continue;
            for (ai->reflected_candidate_level = 0; ai->reflected_candidate_level < BATTLE_MAP_TERRAIN_LAYER_COUNT;
                ai->reflected_candidate_level++) {
                ai->current_action.coords.fields.layer = ai->reflected_candidate_level;
                if ((ai->reachable_tiles[ai->movement_scenario][ai->reflected_candidate_level]
                                        [ai->current_action.coords.fields.y]
                        << ai->current_action.coords.fields.x)
                    & 0x8000) {
                    ai_update_tile_pickup();
                    ai->current_action.act_before_move = 1;
                    ai->reflected_origin_phase = 0;
                    /* Act from the current tile, then move to the mirror tile and act; both
                     * passes test the current tile's origin restrictions. */
                    for (;;) {
                        if (ai_origin_is_allowed(&ai->acting_unit_coords)) {
                        evaluate_targets:
                            if (ai_origin_evaluate_aim_tiles() == -1)
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
    ai_place_acting_unit(&ai->acting_unit_coords);
    ai->current_action.reflected_action = 0;
    ai_rank_insert_best();
    return 0;
}

/* Classify the crystal or treasure occupant of the current action tile. */
void ai_update_tile_pickup(void) {
    ai_data_t* ai = &g_ai_data;
    s32 unit_id;
    unit_stats_t* unit;

    g_ai_data.tile_pickup_kind = AI_TILE_PICKUP_NONE;
    unit_id = ai_find_unit_at_coordinates(&ai->current_action.coords);
    if (unit_id != UNIT_ID_NONE) {
        unit = &g_unit_stats[unit_id];
        if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE) {
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL)) {
                g_ai_data.tile_pickup_kind = AI_TILE_PICKUP_CRYSTAL;
            }
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
                g_ai_data.tile_pickup_kind = AI_TILE_PICKUP_TREASURE;
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
static s32 ai_math_evaluate_targets(void) {
    u8 special_abilities[AI_MATH_EXTRA_KNOWN_ABILITY_COUNT] = { ABILITY_ID_WHITE_MAGIC_CURE,
        ABILITY_ID_WHITE_MAGIC_CURE_2, ABILITY_ID_WHITE_MAGIC_RAISE, ABILITY_ID_BLACK_MAGIC_FIRE,
        ABILITY_ID_BLACK_MAGIC_FIRE_2, ABILITY_ID_BLACK_MAGIC_BOLT, ABILITY_ID_BLACK_MAGIC_BOLT_2,
        ABILITY_ID_BLACK_MAGIC_ICE, ABILITY_ID_BLACK_MAGIC_ICE_2, ABILITY_ID_TIME_MAGIC_DEMI };
    ai_data_t* ai = &g_ai_data;
    s32 i;
    s32 unit;

    if (g_ai_data.resume_pending)
        goto evaluate_priority; /* resume inside the multiplier loop */
    if (!ai_origin_is_allowed(&ai->acting_unit_coords))
        return 0;
    g_ai_data.current_action.act_before_move = 1;
    for (g_ai_math_skillset_index = 0; g_ai_math_skillset_index < AI_MATH_SOURCE_SKILLSET_COUNT;
        g_ai_math_skillset_index++) {
        g_ai_math_skillset_id = g_ai_math_source_skillsets[g_ai_math_skillset_index];
        g_ai_math_ability_list
            = ability_store_skillset_abilities(g_ai_math_skillset_id, SKILLSET_ABILITY_FILTER_ACTION);
        for (g_ai_math_ability_index = 0; g_ai_math_ability_index < HUMAN_SKILLSET_ACTION_SLOT_COUNT;
            g_ai_math_ability_index++) {
            /* Math targeting reads only the low byte of each 16-bit ID. */
            ai->math_ability_id = g_ai_math_ability_list[g_ai_math_ability_index];
            if (ai->math_ability_id == 0)
                continue;
            if (!(g_ability_range_data[ai->math_ability_id].flags_3 & ABILITY_SECONDARY_FLAG_3_CALCULATOR_ELIGIBLE))
                continue;
            if (!ai_ability_is_known(ai->acting_unit_id, g_ai_math_skillset_id, g_ai_math_ability_index))
                continue;
            /* The extra known-bit call's result is ignored. */
            for (i = 0; i < AI_MATH_EXTRA_KNOWN_ABILITY_COUNT; i++) {
                if (ai->math_ability_id == special_abilities[i])
                    ai_ability_is_known(ai->acting_unit_id, g_ai_math_skillset_id, g_ai_math_ability_index + 1);
            }
            ai_mem_copy_u16_values(&ai->considered_ability.ai_flags, &g_ability_data[ai->math_ability_id].ai_flags, 4);
            ai_mem_copy_bytes(ai->considered_ability.parameters.fields.status_infliction,
                g_status_infliction_data[g_ability_range_data[ai->math_ability_id].status_infliction_id].statuses, 5);
            ai->considered_ability.ai_flags.fields.flags_4 = AI_ABILITY_FLAG_4_USABLE_BY_AI;
            ai->current_action.command.ability_id = ai->math_ability_id;
            ai->considered_ability.element = g_ability_range_data[ai->math_ability_id].element;
            for (unit = 0; unit < UNIT_SLOT_COUNT; unit++)
                ai->targetability.live.unit_targetable[unit] = ai->targetability.live.unit_active[unit];
            if (!ai_target_filter_useful())
                continue;
            for (ai->math_type_index = 0; ai->math_type_index < CALCULATOR_ATTRIBUTE_ABILITY_COUNT;
                ai->math_type_index++) {
                if (!ai_ability_is_known(ai->acting_unit_id, ai->considered_ability.skillset, ai->math_type_index))
                    continue;
                ai->math_multiplier_index = CALCULATOR_ATTRIBUTE_ABILITY_COUNT;
                ai->current_action.command.calculator_type = ai->math_type_index + ABILITY_ID_MATH_FIRST;
                do {
                    if (ai_ability_is_known(
                            ai->acting_unit_id, ai->considered_ability.skillset, ai->math_multiplier_index)) {
                        ai->current_action.command.calculator_multiplier
                            = ai->math_multiplier_index + ABILITY_ID_MATH_FIRST;
                    evaluate_priority:
                        if (ai_evaluate_current_action() == -1)
                            return -1;
                    }
                    ai->math_multiplier_index++;
                } while (ai->math_multiplier_index < CALCULATOR_ABILITY_COUNT);
            }
            ai_rank_insert_best();
        }
    }
    ai_place_acting_unit(&ai->acting_unit_coords);
    return 0;
}

/*
 * Evaluate target tiles from eligible movement origins.
 *
 * Return -1 with the search stage saved when time or a child suspends;
 * return 0 after restoring the temporary unit's position. The scan path
 * reloads its full row mask on resume and continues at its saved x index.
 *
 * An exhaustive-origin ability visits every cast origin in map order (with
 * max_possibilities as the x counter). Any other ability takes origins best
 * first by tile score while both the aim-tile and cast-origin counts last.
 */
static s32 ai_origin_evaluate_reachable(void) {
    ai_data_t* ai = &g_ai_data;
    s32 row;

    if (g_ai_data.resume_pending) {
        /* Resume: re-enter the step that suspended. */
        if (g_ai_data.origin_evaluation_stage == AI_ORIGIN_EVALUATION_PREPARE)
            goto begin_evaluation;
        if (g_ai_data.origin_evaluation_stage == AI_ORIGIN_EVALUATION_RANKED)
            goto evaluate_ranked_origin;
        row = ai->cast_origin_tiles[g_ai_data.map_level_counter][g_ai_data.y_counter];
        goto evaluate_scanned_origin;
    }
    row = ai->targetability.live.unit_targetable[g_ai_data.acting_unit_id];
    ai->targetability.live.unit_targetable[g_ai_data.acting_unit_id] = 0;
    ai_origin_build_cast_and_aim_tiles();
    ai->targetability.live.unit_targetable[g_ai_data.acting_unit_id] = row;
begin_evaluation:
    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        ai->origin_evaluation_stage = AI_ORIGIN_EVALUATION_PREPARE;
        return -1;
    }
    ai->resume_pending = 0;
    ai->ranked_origin_scan = 0;
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_EXHAUSTIVE_ORIGIN_SCAN) {
        ai->targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_CONSUME_EVALUATED_AIM_TILES;
    } else {
        ai->max_possibilities = ai->aim_tile_count;
        ai->max_possibilities *= ai->cast_origin_tile_count;
        if (ai->max_possibilities > 10)
            ai->ranked_origin_scan = 1;
        /* The target sets this regardless of the truncated product test. */
        ai->ranked_origin_scan = 1;
    }
    if ((ai->targetability.live.target_setting_flags & AI_TARGET_SETTING_CONSUME_EVALUATED_AIM_TILES)
        || ai->ranked_origin_scan) {
        while (ai->aim_tile_count && ai->cast_origin_tile_count) {
            ai_origin_take_next();
            ai_origin_finalize_destination();
        evaluate_ranked_origin:
            row = ai_origin_evaluate_aim_tiles();
            if (row == -1) {
                ai->origin_evaluation_stage = AI_ORIGIN_EVALUATION_RANKED;
                return -1;
            }
        }
    } else {
        ai->map_level_counter = 0;
        do {
            for (ai->y_counter = 0; ai->y_counter < ai->terrain_depth; ai->y_counter++) {
                row = ai->cast_origin_tiles[ai->map_level_counter][ai->y_counter];
                if (row != 0) {
                    for (ai->max_possibilities = 0; ai->max_possibilities < ai->terrain_width;
                        ai->max_possibilities++) {
                        if ((row << ai->max_possibilities) & 0x8000) {
                            ai->current_action.coords.fields.x = ai->max_possibilities;
                            ai->current_action.coords.fields.y = ai->y_counter;
                            ai->current_action.coords.fields.layer = ai->map_level_counter;
                            ai_origin_finalize_destination();
                        evaluate_scanned_origin:
                            if (ai_origin_evaluate_aim_tiles() == -1) {
                                ai->origin_evaluation_stage = AI_ORIGIN_EVALUATION_SCANNED;
                                return -1;
                            }
                        }
                    }
                }
            }
            ai->map_level_counter++;
        } while (ai->map_level_counter < BATTLE_MAP_TERRAIN_LAYER_COUNT);
    }
    ai_place_acting_unit(&ai->acting_unit_coords);
    return 0;
}

/*
 * Mark an actor-origin choice and check the resulting tile for crystal or treasure.
 *
 * An actor-origin choice sets act_before_move and substitutes the movement
 * scenario's candidate coordinates; other origins clear that flag.
 */
static void ai_origin_finalize_destination(void) {
    ai_data_t* ai = &g_ai_data;

    if (g_ai_data.current_action.coords.word != g_ai_data.acting_unit_coords.word) {
        g_ai_data.current_action.act_before_move = 0;
    } else {
        g_ai_data.current_action.act_before_move = 1;
        g_ai_data.current_action.coords = ai->candidate_coords[g_ai_data.movement_scenario];
    }
    ai_update_tile_pickup();
}

/*
 * Take the lowest-key remaining movement origin for target evaluation.
 *
 * Rank by signed tile priority, then foe proximity; later tiles win ties.
 * Requires a consistent nonempty cast-origin mask/count; there is no empty guard.
 */
static void ai_origin_take_next(void) {
    ai_data_t* ai = &g_ai_data;
    s32 best_key = 0x7fffffff;
    s32 x, y, level;
    s32 key;
    u16 row;

    for (level = 0; level < BATTLE_MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->terrain_depth; y++) {
            row = ai->cast_origin_tiles[level][y];
            if (row != 0) {
                for (x = 0; x < ai->terrain_width; x++) {
                    if ((row << x) & 0x8000) {
                        s32 priority_scale = 65536;
                        key = ai->tile_priority[level][y][x];
                        key *= priority_scale;
                        key += ai->tile_foe_proximity[level][y][x];
                        if (best_key >= key) {
                            best_key = key;
                            ai->current_action.coords.fields.x = x;
                            ai->current_action.coords.fields.y = y;
                            ai->current_action.coords.fields.layer = level;
                        }
                    }
                }
            }
        }
    }
    ai->cast_origin_tiles[ai->current_action.coords.fields.layer][ai->current_action.coords.fields.y]
        ^= 0x8000 >> ai->current_action.coords.fields.x;
    ai->cast_origin_tile_count--;
}

/* Build the cast-origin and aim tile masks for the considered ability.
 *
 * cast_origin_tiles are the scenario's reachable tiles to cast from: for an arcing
 * ability all of them; otherwise those with a targetable unit within range
 * (range plus area when aiming at tiles), dropping deep water for a unit that
 * cannot enter it, for Geomancy the tiles whose terrain gives another ability,
 * and for a Monster Skill ability the tiles out of a holder's reach.
 * aim_tiles are the walkable tiles a cast can be aimed at to hit a
 * targetable unit: its own tile for unit targeting, any tile within the area
 * otherwise. An ability that follows its target cannot be aimed at a unit's
 * own tile when aiming at tiles or when the unit is jumping. Both counts are
 * stored. */
void ai_origin_build_cast_and_aim_tiles(void) {
    ai_data_t* ai = &g_ai_data;
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
        ai->aim_tiles[0][i] = 0;
        ai->aim_tiles[1][i] = 0;
    }

    /* Tiles to cast from; aim_tiles is scratch here for every tile in range of a target. */
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_ARC_TRAJECTORY) {
        ai_mem_copy_u16_values(
            ai->cast_origin_tiles, ai->reachable_tiles[ai->movement_scenario][0], sizeof(ai->cast_origin_tiles));
    } else {
        for (unit_id = 0; unit_id < UNIT_SLOT_COUNT; unit_id++) {
            if (ai->targetability.live.unit_targetable[unit_id]) {
                ai_copy_unit_coordinates(unit_id, &ai->considered_unit_coords);
                if (ai->current_action.command.targeting_type == TARGET_TYPE_UNIT)
                    ai_origin_build_targetable_mask(AI_TILE_MASK_RANGE, 0);
                else
                    ai_origin_build_targetable_mask(AI_TILE_MASK_RANGE_PLUS_AOE, 0);
                for (i = 0; i < ai->terrain_depth; i++) {
                    ai->aim_tiles[0][i] |= ai->targetable_tiles[0][i];
                    ai->aim_tiles[1][i] |= ai->targetable_tiles[1][i];
                }
            }
        }
        for (i = 0; i < ai->terrain_depth; i++) {
            ai->cast_origin_tiles[0][i] = ai->reachable_tiles[ai->movement_scenario][0][i] & ai->aim_tiles[0][i];
            ai->cast_origin_tiles[1][i] = ai->reachable_tiles[ai->movement_scenario][1][i] & ai->aim_tiles[1][i];
            ai->aim_tiles[0][i] = 0;
            ai->aim_tiles[1][i] = 0;
        }
        for (i = 0; i < BATTLE_MAP_TERRAIN_LAYER_COUNT; i++) {
            for (j = 0; j < ai->terrain_depth; j++) {
                unit_id = ai->cast_origin_tiles[i][j];
                if (unit_id == 0)
                    continue;
                for (x = 0; x < ai->terrain_width; x++) {
                    if ((unit_id << x) & 0x8000) {
                        if (ai->water_penalty
                            && (depth = g_battle_map_tile_data[j * ai->terrain_width + x
                                    + (i * BATTLE_MAP_TILES_PER_LAYER)]
                                       .depth_half_height.depth)
                                >= 2) {
                            ai->cast_origin_tiles[i][j] ^= 0x8000 >> x;
                        } else if (g_command_menu_types_by_skillset[ai->considered_ability.skillset]
                            == ACTION_MENU_TYPE_ELEMENTS) {
                            ai->considered_unit_coords.fields.x = x;
                            ai->considered_unit_coords.fields.y = j;
                            ai->considered_unit_coords.fields.layer = i;
                            if (!ai_origin_can_use_elemental(&ai->considered_unit_coords))
                                ai->cast_origin_tiles[i][j] ^= 0x8000 >> x;
                        }
                    }
                }
            }
        }
        if (ai->considered_ability.ai_flags.word & AI_ABILITY_REQUIRES_MONSTER_SKILL) {
            ai_origin_build_monster_skill_mask();
            for (i = 0; i < ai->terrain_depth; i++) {
                ai->cast_origin_tiles[0][i] &= ai->targetable_tiles[0][i];
                ai->cast_origin_tiles[1][i] &= ai->targetable_tiles[1][i];
            }
        }
    }

    /* Aim tiles: each targetable unit's tile, or the tiles whose area covers it. */
    for (unit_id = 0; unit_id < UNIT_SLOT_COUNT; unit_id++) {
        if (ai->targetability.live.unit_targetable[unit_id]) {
            ai_copy_unit_coordinates(unit_id, &ai->considered_unit_coords);
            if (ai->current_action.command.targeting_type == TARGET_TYPE_UNIT)
                ai_origin_build_targetable_mask(AI_TILE_MASK_EXPLICIT, 0);
            else
                ai_origin_build_targetable_mask(AI_TILE_MASK_AOE, 0);
            for (i = 0; i < ai->terrain_depth; i++) {
                ai->aim_tiles[0][i] |= ai->targetable_tiles[0][i];
                ai->aim_tiles[1][i] |= ai->targetable_tiles[1][i];
            }
        }
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_FOLLOW_TARGET) {
        for (unit_id = 0; unit_id < UNIT_SLOT_COUNT; unit_id++) {
            if (ai->targetability.live.unit_targetable[unit_id]) {
                unit = &g_unit_stats[unit_id];
                status = unit->status_sets.current[0];
                /* Keeps the position reads on a copy of the loop pointer (`move a1,a2`) that cse2 would fold. */
                pos = unit;
                __asm__("" : "=r"(pos) : "0"(pos));
                jumping = status & STATUS_BYTE_MASK(STATUS_ID_JUMP);
                if (ai->current_action.command.targeting_type == TARGET_TYPE_TILE || jumping) {
                    ai->aim_tiles[pos->position.higher_elevation][pos->position.y] ^= 0x8000 >> pos->x;
                }
            }
        }
    }
    for (i = 0; i < BATTLE_MAP_TERRAIN_LAYER_COUNT; i++) {
        for (j = 0; j < ai->terrain_depth; j++) {
            ai->aim_tiles[i][j] &= ai->walkable_tiles[i][j];
        }
    }
    ai->aim_tile_count = ai_count_flagged_tiles_within_map_bounds(ai->aim_tiles);
    ai->cast_origin_tile_count = ai_count_flagged_tiles_within_map_bounds(ai->cast_origin_tiles);
}

/*
 * Build the considered origin's targetable-tile mask on both map levels.
 *
 * The range, area and range-plus-area modes take their radius from the
 * considered ability; AI_TILE_MASK_EXPLICIT uses the radius argument. The
 * diamond is built from row templates centered on column 7 and shifted to the
 * origin's column, then copied to the upper level; a linear ability keeps
 * only the origin's row and column.
 * Radius 16 or greater fills both masks without applying the linear filter.
 * Radii 8 through 15 read the following status-priority weights instead of
 * row templates. Jump Level 8 can supply radius 8 on non-arcing origin scans.
 * The lookup is unchecked.
 */
static void ai_origin_build_targetable_mask(ai_tile_mask_mode_e mode, s32 radius) {
    ai_data_t* ai = &g_ai_data;
    s32 i;
    s32 x;
    s32 y;

    /* Preserve the target's signed mode dispatch. */
    switch ((s32)mode) {
    case AI_TILE_MASK_RANGE:
        radius = g_ai_data.considered_ability.parameters.fields.range;
        break;
    case AI_TILE_MASK_AOE:
        radius = g_ai_data.considered_ability.parameters.fields.aoe;
        break;
    case AI_TILE_MASK_RANGE_PLUS_AOE:
        radius
            = g_ai_data.considered_ability.parameters.fields.range + g_ai_data.considered_ability.parameters.fields.aoe;
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
            ai->targetable_tiles[0][y - i] = g_ai_origin_range_row_masks[radius - i] << (7 - x);
        else
            ai->targetable_tiles[0][y - i] = g_ai_origin_range_row_masks[radius - i] >> (x - 7);
    }
    for (i = 1; i <= radius; i++) {
        if (y + i >= ai->terrain_depth)
            break;
        if (x < 8)
            ai->targetable_tiles[0][y + i] = g_ai_origin_range_row_masks[radius - i] << (7 - x);
        else
            ai->targetable_tiles[0][y + i] = g_ai_origin_range_row_masks[radius - i] >> (x - 7);
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_LINEAR_TRAJECTORY) {
        for (y = 0; y < ai->terrain_depth; y++) {
            i = ai->targetable_tiles[0][y];
            if (i != 0) {
                for (x = 0; x < ai->terrain_width; x++) {
                    if ((i << x) & 0x8000) {
                        if (x != ai->considered_unit_coords.fields.x && y != ai->considered_unit_coords.fields.y)
                            ai->targetable_tiles[0][y] ^= 0x8000 >> x;
                    }
                }
            }
        }
    }
    ai_mem_copy_u16_values(ai->targetable_tiles[1], ai->targetable_tiles[0], sizeof(ai->targetable_tiles[0]));
}

/*
 * Build the targetable-tile mask around same-team Monster Skill holders.
 *
 * Only the low map level is visited; the high-level mask remains cleared.
 * Tile base heights must differ by at most 2; slope and depth are ignored.
 * The offsets are unsigned bytes, so the -1 of the south and west neighbors
 * adds 255 and fails the bounds test: only the north and east neighbors are
 * ever marked (QUIRKS.md).
 */
static void ai_origin_build_monster_skill_mask(void) {
    ai_data_t* ai = &g_ai_data;
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
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE)
            continue;
        if (!(unit->ability_flags.support[3] & UNIT_SUPPORT_SET_4_MONSTER_SKILL))
            continue;
        if ((unit->initial_team_flags & UNIT_TEAM_MASK) != ai->acting_unit_team)
            continue;
        if (status_unit_has_any_in_set(unit, STATUS_CHECK_SET_PREVENT_REACTION))
            continue;
        height = g_battle_map_tile_data[unit->position.higher_elevation * BATTLE_MAP_TILES_PER_LAYER
            + unit->position.y * ai->terrain_width + unit->x]
                     .height;
        for (direction = 0; direction < AI_CARDINAL_DIRECTION_COUNT; direction++) {
            x = unit->x + g_ai_facing_tile_offsets.by_direction[direction][0];
            if (x < 0 || x >= ai->terrain_width)
                continue;
            y = unit->position.y + g_ai_facing_tile_offsets.by_direction[direction][1];
            if (y < 0 || y >= ai->terrain_depth)
                continue;
            for (level = 0; level <= 0; level++) {
                s32 rise
                    = g_battle_map_tile_data[(level * BATTLE_MAP_TILES_PER_LAYER) + y * ai->terrain_width + x].height
                    - height;
                if (rise >= -2 && rise <= 2) {
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
static s32 ai_origin_evaluate_aim_tiles(void) {
    ai_data_t* ai = &g_ai_data;
    s32 x, y, level;
    u16 row;
    ai_coords_t* origin;
    s32 i;
    battle_map_tile_t* tile;
    u16* target_rows;
    u16* origin_rows;
    s32 tile_mask;
    u32 level_address;
    u8 target_id;

    if (g_ai_data.resume_pending && g_ai_data.search_resume_flag) {
        x = (u16)g_ai_data.current_action.command.target_position.vx;
        level = (u16)g_ai_data.current_action.command.target_position.vy;
        y = (u16)g_ai_data.current_action.command.target_position.vz;
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
    ai_place_acting_unit(origin);
    if (ai->considered_ability.parameters.fields.range) {
        if (target_range_set_for_action(&ai->current_action.command) == TARGET_PANELS_NONE)
            return 0;
    } else {
        tile = g_battle_map_tile_data;
        for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++, tile++)
            tile->ceiling_depth_and_marks.ability_range = 0;
        g_battle_map_tile_data[origin->fields.y * ai->terrain_width + origin->fields.x
            + (origin->fields.layer * BATTLE_MAP_TILES_PER_LAYER)]
            .ceiling_depth_and_marks.ability_range = 1;
    }

    /* Field-derived level bases keep both row pointers in the target order;
     * typed level indexing adds instructions to this intersection loop. */
    for (level = 0, level_address = (u32)ai; level < BATTLE_MAP_TERRAIN_LAYER_COUNT;
        level++, level_address += AI_GRID_ROW_COUNT * sizeof(u16)) {
        for (y = 0; y < ai->terrain_depth; y++) {
            target_rows = (u16*)(level_address + ((u32)&ai->targetable_tiles - (u32)ai));
            origin_rows = (u16*)(level_address + ((u32)&ai->aim_tiles - (u32)ai));
            row = origin_rows[y];
            target_rows[y] = 0;
            if (row != 0) {
                for (x = 0; x < ai->terrain_width; x++) {
                    /* Keep the membership test and update in one working mask. */
                    tile_mask = row << x;
                    if (tile_mask & 0x8000) {
                        if (g_battle_map_tile_data[y * ai->terrain_width + x + (level * BATTLE_MAP_TILES_PER_LAYER)]
                                .ceiling_depth_and_marks.ability_range) {
                            tile_mask = 0x8000;
                            target_rows[y] |= tile_mask >> x;
                        }
                    }
                }
            }
        }
    }
    for (level = 0; level < BATTLE_MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->terrain_depth; y++) {
            row = ai->targetable_tiles[level][y];
            if (row != 0) {
                for (x = 0; x < ai->terrain_width; x++) {
                    if ((row << x) & 0x8000) {
                        ai->current_action.command.target_position.vx = x;
                        ai->current_action.command.target_position.vz = y;
                        ai->current_action.command.target_position.vy = level;
                        if (ai->current_action.command.targeting_type == TARGET_TYPE_UNIT
                            || (ai->considered_ability.ai_flags.word
                                & (AI_ABILITY_DIRECT_TRAJECTORY | AI_ABILITY_ARC_TRAJECTORY))) {
                            target_id = ai_target_find_unit_on_panel();
                            ai->current_action.command.target_id = target_id;
                            if (target_id == UNIT_ID_NONE)
                                goto remove_aim_tile;
                        }
                        if (ai->considered_ability.ai_flags.word & AI_ABILITY_ARC_TRAJECTORY) {
                            if (projectile_arc_check_between_units(
                                    ai->acting_unit_id, ai->current_action.command.target_id)
                                == 0)
                                continue;
                        } else if (ai->considered_ability.ai_flags.word & AI_ABILITY_DIRECT_TRAJECTORY) {
                            /* A thrown item needs a clear path only to a target beyond the next tile. */
                            if ((g_command_menu_types_by_skillset[ai->considered_ability.skillset]
                                        != ACTION_MENU_TYPE_ITEM_INVENTORY
                                    || ai_calculate_distance_between_units(
                                           ai->acting_unit, &g_unit_stats[ai->current_action.command.target_id])
                                        >= 2)
                                && projectile_direct_check_between_units(
                                       ai->acting_unit_id, ai->current_action.command.target_id)
                                    == 0)
                                continue;
                        }
                        if (ai->targetability.live.target_setting_flags & AI_TARGET_SETTING_CONSUME_EVALUATED_AIM_TILES)
                            ai->consume_aim_tile = 1;
                        else
                            ai->consume_aim_tile = 0;
                    evaluate_priority:
                        if (ai_evaluate_current_action() == -1) {
                            ai->search_resume_flag = 1;
                            return -1;
                        }
                        /* Give up this origin when the simulation never resolved the acting unit's action. */
                        if (ai->ability_processing_done == 0)
                            return 0;
                    remove_aim_tile:
                        /* Consume the aim tile (CONSUME_EVALUATED_AIM_TILES, or a caster outcome the strike
                         * simulation flagged) so later origins skip it. */
                        if (ai->consume_aim_tile) {
                            ai->aim_tiles[level][y] ^= 0x8000 >> x;
                            ai->aim_tile_count--;
                            if (ai->aim_tile_count == 0)
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
 * panel, or UNIT_ID_NONE if none. */
static s32 ai_target_find_unit_on_panel(void) {
    ai_data_t* ai = &g_ai_data;
    s32 i;
    unit_stats_t* unit;
    u16 x;
    u16 y;
    u16 elevation;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
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

/* Return the first present unit's slot at these coordinates, or UNIT_ID_NONE if absent. */
s32 ai_find_unit_at_coordinates(const ai_coords_t* coords) {
    s32 i;
    unit_stats_t* unit;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE && unit->x == coords->fields.x
            && unit->position.y == coords->fields.y && (unit->position.higher_elevation) == coords->fields.layer) {
            break;
        }
    }
    return i;
}

/* Simulate and score the current action and keep it as the best when it
 * outranks the best so far. -1 suspends; otherwise 0. */
s32 ai_evaluate_current_action(void) {
    ai_data_t* ai = &g_ai_data;
    s32 result = ai_simulate_action_and_score();

    if (result == -1) {
        return -1;
    }
    if (result != 0) {
        if (ai_rank_is_higher(&ai->current_action, &ai->best_action) != 0) {
            ai_mem_copy_u16_values(&ai->best_action, &ai->current_action, sizeof(ai_action_data_t));
        }
    }
    return 0;
}

enum { AI_HP_SCORE_SCALE = 128, AI_MP_SCORE_SCALE = 64, AI_EQUIPMENT_SCORE = 51, AI_GOLEM_SCORE_SCALE = 128 };

/*
 * Simulate the current action and intervening events, then score unit state.
 *
 * Return -1 to suspend, 0 for a rejected or flag-update-only pass, and 1
 * after scoring. Save/restore helpers preserve selected unit data, not the
 * whole AI workspace; resumed phases skip fresh action preparation.
 *
 * The candidate's move and action are applied to the live unit records in its
 * order (an immediate action that hits nothing rejects it). The turn then ends
 * and the turn order runs, resolving charged actions and noting who gets a
 * turn first, until nobody is charging or performing or 256 events pass. The
 * outcome is scored into the action's priority: for each unit its HP share,
 * status weights, kept equipment, MP share and team Golem, negated for
 * enemies. Every unit's data is then restored from the baseline snapshot.
 */
s32 ai_simulate_action_and_score(void) {
    ai_data_t* ai = &g_ai_data;
    unit_stats_t* unit;
    ai_unit_decision_t* decision;
    action_command_t* action;
    u32 clear_address;
    u8* decision_base;
    s32 unit_index;
    s32 status;
    s32 equipment_slot;
    s32 weight_offset;
    s32 weight;
    s32 score;
    u32 below_cost;

    if (g_ai_data.resume_pending && g_ai_data.action_simulation_phase) {
        ai_simulate_restore_unit_state(AI_SNAPSHOT_SIMULATED_STATUS);
        /* Resume: re-enter the suspended turn-advance or scoring phase. */
        if (g_ai_data.action_simulation_phase == AI_SIMULATION_ADVANCE_TURNS)
            goto advance_turns;
        if (g_ai_data.action_simulation_phase == AI_SIMULATION_SCORE_STATE)
            goto score_state;
    }

    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        ai->action_simulation_phase = AI_SIMULATION_PREPARE_ACTION;
        return -1;
    }

    /* Every exit below restores this baseline, undoing the simulation. */
    ai_simulate_save_unit_state(AI_SNAPSHOT_BASELINE);
    ai->ability_processing_done = 0;
    /* Integer addresses retain the original base/displacement order without forming
     * a pointer before the workspace at the end of the reverse walk. */
    for (unit_index = UNIT_SLOT_COUNT - 1, clear_address = (u32)ai + (UNIT_SLOT_COUNT - 1); unit_index >= 0;
        unit_index--, clear_address--) {
        *(u8*)(clear_address + offsetof(ai_data_t, unit_had_move_opportunity)) = 0;
    }
    ai->current_action.rank.priority = 0;
    ai->current_action.rank.base_hit_percent = 0xff;

    /* Simulate the action and the move in the candidate's order. An immediate action
     * that hits nothing rejects the candidate. */
    if (ai->current_action.act_before_move) {
        if (ai->current_action.command.skillset_id != 0
            && ai->current_action.command.skillset_id != SKILLSET_ID_DEFEND) {
            action = &ai->current_action.command;
            if (action_commit_preparation(action) == ACTION_PREPARATION_IMMEDIATE
                && ai_simulate_action_hits(action) == 0) {
                ai_simulate_restore_unit_state(AI_SNAPSHOT_BASELINE);
                ai->resume_pending = 0;
                return 0;
            }
            ai->acting_unit->action_taken = 1;
            ai_simulate_movement_and_pickup();
        }
    } else {
        ai_simulate_movement_and_pickup();
        if (ai->current_action.command.skillset_id != 0
            && ai->current_action.command.skillset_id != SKILLSET_ID_DEFEND) {
            action = &ai->current_action.command;
            if (action_commit_preparation(action) == ACTION_PREPARATION_IMMEDIATE
                && ai_simulate_action_hits(action) == 0) {
                ai_simulate_restore_unit_state(AI_SNAPSHOT_BASELINE);
                ai->resume_pending = 0;
                return 0;
            }
            ai->acting_unit->action_taken = 1;
        }
    }

    if (ai->current_action.command.skillset_id == SKILLSET_ID_DEFEND) {
        status_inflict_defending(ai->acting_unit_id);
        ai->current_action.rank.base_hit_percent = 100;
        ai->acting_unit->action_taken = 1;
    }
    if (ai->current_action.command.skillset_id == 0)
        ai->current_action.rank.base_hit_percent = 100;

    /* End the turn and save the between-turn globals the simulation overwrites. */
    turn_end(ai->acting_unit_id);
    ai->saved_between_turn_state = g_turn_clock_state;
    ai->saved_between_turn_resume_state = g_turn_clock_resume_state;
    ai->saved_turn_unit_id = g_turn_unit_id;
    ai->simulated_turn_events = 0;

    /* Run the turn order until no unit is charging or performing, or the event limit hits. */
    for (;;) {
    advance_turns:
        if (VSync(1) >= AI_TURN_SIMULATION_SCANLINE_LIMIT) {
            ai_simulate_save_unit_state(AI_SNAPSHOT_SIMULATED_STATUS);
            ai->action_simulation_phase = AI_SIMULATION_ADVANCE_TURNS;
            return -1;
        }
        ai->simulated_turn_events++;
        if (ai->simulated_turn_events >= AI_SIMULATED_TURN_LIMIT)
            break;

        /* The control returns the turn event in bits 8..15 and its unit in the low byte. */
        unit_index = turn_run_clock(TURN_CLOCK_MODE_RUN);
        status = unit_index & 0xff00;
        unit_index &= 0xff;
        unit = &g_unit_stats[unit_index];
        if (status == TURN_EVENT_NONE)
            break;
        if (status == TURN_EVENT_ACTION_RESULT) {
            action_apply_target_result(unit_index);
        } else if (status == TURN_EVENT_ABILITY_READY) {
            if (ai_simulate_action_hits(&unit->action_command) == 0) {
                g_turn_clock_state = ai->saved_between_turn_state;
                g_turn_clock_resume_state = ai->saved_between_turn_resume_state;
                g_turn_unit_id = ai->saved_turn_unit_id;
                /* An early return duplicates this shared cleanup tail. */
                goto finish_without_scoring;
            }
            unit->charged_ability_ct = 0xff;
            unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                &= ~(STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
            unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                &= ~(STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
        } else if (status == TURN_EVENT_UNIT_READY) {
            if (ai_is_unit_movement_eligible_after_delay(0, unit_index))
                ai->unit_had_move_opportunity[unit_index] = 1;
            unit->has_turn = 0;
        }

        for (unit_index = 0; unit_index < UNIT_SLOT_COUNT; unit_index++) {
            unit = &g_unit_stats[unit_index];
            if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                    & (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING))))
                break;
        }
        if (unit_index == UNIT_SLOT_COUNT)
            break;
    }

    g_turn_clock_state = ai->saved_between_turn_state;
    g_turn_clock_resume_state = ai->saved_between_turn_resume_state;
    g_turn_unit_id = ai->saved_turn_unit_id;
score_state:
    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        ai_simulate_save_unit_state(AI_SNAPSHOT_SIMULATED_STATUS);
        ai->action_simulation_phase = AI_SIMULATION_SCORE_STATE;
        return -1;
    }

    /* A refresh pass only updates each unit's transient status flags and returns 0. */
    if (ai->targetability.live.target_setting_flags & AI_TARGET_SETTING_REFRESH_UNIT_STATUS_FLAGS) {
        for (unit_index = 0; unit_index < UNIT_SLOT_COUNT; unit_index++) {
            decision = &ai->unit_decisions[unit_index];
            unit = &g_unit_stats[unit_index];
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
        ai_rank_clear_current_decision();
        /* Score every unit on HP, statuses, kept equipment, MP and Golem; enemies count negative. */
        for (unit_index = 0; unit_index < UNIT_SLOT_COUNT; unit_index++) {
            status = ai->unit_compact_indices[unit_index];
            if (status == 0xff)
                continue;
            unit = &g_unit_stats[unit_index];
            score = unit->hp;
            score *= AI_HP_SCORE_SCALE;
            score /= unit->max_hp;

            status = 0;
            /* A pointer to unit_decisions adds an addiu and changes scheduling;
             * retain the workspace-relative stride. */
            decision_base = (u8*)ai + unit_index * sizeof(ai_unit_decision_t);
            for (weight_offset = 0; status < STATUS_COUNT; status++, weight_offset += 2) {
                if (!((unit->status_sets.current[status / 8] << (status % 8)) & 0x80))
                    continue;
                /* Indexing the s16 table changes register allocation; retain the byte cursor. */
                weight = *(s16*)((u8*)g_ai_status_priority_weights + weight_offset);
                /* A confused, blood-sucked or charmed unit gets no credit for positive status weights. */
                if (((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
                         & (STATUS_BYTE_MASK(STATUS_ID_CONFUSION) | STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)))
                        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)]
                            & STATUS_BYTE_MASK(STATUS_ID_CHARM)))
                    && weight >= 0)
                    continue;
                switch (status) {
                case STATUS_ID_DARKNESS:
                    weight = weight * decision_base[offsetof(ai_data_t, unit_decisions[0].evade_mod)] / 4;
                    break;
                case STATUS_ID_SILENCE:
                    weight = weight * decision_base[offsetof(ai_data_t, unit_decisions[0].silence_mod)] / 4;
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

            /* MP share, halved when MP is below the unit's cheapest MP ability. */
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

            /* Golem HP counts like shared team HP; any loss since the baseline snapshot costs one more point. */
            {
                u32 golem;
                s32 team;

                team = (unit->team_flags >> UNIT_TEAM_SHIFT) & UNIT_TEAM_INDEX_MASK;
                weight_offset = g_action_team_golem[team];
                golem = weight_offset;
                weight_offset *= AI_GOLEM_SCORE_SCALE;
                score += weight_offset / ai->present_unit_average_max_hp;
                if (golem < ai->team_golem[team])
                    score--;
            }

            if (ai->unit_decisions[unit_index].enemy_flag)
                score = -score;
            ai->current_action.rank.priority += score;
        }

        /* Throwing anything but a shuriken or a ball halves the hit percent, which lowers
         * the action's rank and its chance to be picked. */
        if (g_command_menu_types_by_skillset[ai->considered_ability.skillset] == ACTION_MENU_TYPE_WEAPON_INVENTORY
            && ai->considered_ability.ability_id != ABILITY_ID_THROW_SHURIKEN
            && ai->considered_ability.ability_id != ABILITY_ID_THROW_BALL)
            ai->current_action.rank.base_hit_percent >>= 1;
        ai_simulate_restore_unit_state(AI_SNAPSHOT_BASELINE);
        ai->resume_pending = 0;
        return 1;
    }
finish_without_scoring:
    ai_simulate_restore_unit_state(AI_SNAPSHOT_BASELINE);
    ai->resume_pending = 0;
    return 0;
}

/*
 * Simulate movement and its crystal or treasure benefit.
 *
 * An unchanged 32-bit coordinate value skips all updates. Crystal recovery clears
 * Critical; a noncritical pickup adds priority unless the policy is Retreat.
 */
static void ai_simulate_movement_and_pickup(void) {
    ai_data_t* ai = &g_ai_data;

    if (ai->current_action.coords.word != ai->acting_unit_coords.word) {
        ai_place_acting_unit(&ai->current_action.coords);
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
static void ai_rank_clear_current_decision(void) {
    g_ai_data.current_action.rank.decided = 0;
}

/*
 * Simulate an action and finalize its hit-percentage average when requested.
 *
 * Accounting starts only for the acting unit with an unset base hit rate.
 * The 0xff counter disables accounting; that path returns 1 without
 * requiring a valuable target. Counted entries are target/strike pairs.
 */
static s32 ai_simulate_action_hits(action_command_t* action) {
    ai_data_t* ai = &g_ai_data;
    s32 base_hit;
    s32 cnt;

    g_ai_data.hit_counter = 0xFF;
    if (action->actor_id == g_ai_data.acting_unit_id) {
        base_hit = g_ai_data.current_action.rank.base_hit_percent;
        g_ai_data.ability_processing_done = 1;
        if (base_hit == 0xFF) {
            g_ai_data.hit_counter = 0;
            g_ai_data.total_hit_percent = 0;
            g_ai_data.valuable_target_hit = 0;
        }
    }
    ai_simulate_strikes(action, 0);
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
 * can adjust priority and mark whether a valuable target was hit. A hit on the
 * side the ability is meant for adds `positive` to the priority and one on the
 * other side subtracts `negative`: 0 and 1, or 3 and 4 for an ability that
 * affects stats.
 */
static void ai_simulate_strikes(action_command_t* action, s32 reaction_mode) {
    ai_data_t* ai = &g_ai_data;
    action_strike_work_t work;
    s32 target_index;
    s32 target_id;
    action_data_t* result;
    ability_ai_flags_t* behavior;
    s32 self_hit;
    s32 positive;
    s32 negative;

    if (reaction_mode == 0 && action_init_current_data(action->actor_id) != 0) {
        return;
    }
    behavior = &g_ability_data[action->ability_id].ai_flags;
    if (g_ai_data.hit_counter != 0xff) {
        self_hit = 0;
        positive = 0;
        negative = 1;
        if (behavior->flags_1 & AI_ABILITY_AFFECTS_STATS) {
            positive = 3;
            negative = 4;
        }
    }
    do {
        if (action_resolve_strike(action->actor_id, &work) == 0 && work.target_count != 0) {
            /* The target scans only for 0xff, without a count or 16-entry bound. */
            for (target_index = 0; work.target_list[target_index] != 0xff; target_index++) {
                target_id = work.target_list[target_index];
                result = &g_unit_stats[target_id].action;
                if (result->attack_accuracy <= 0) {
                    continue;
                }
                if (!(behavior->flags_2 & AI_ABILITY_FLAG_2_TARGET_MAP_TILES)) {
                    if (ai_target_is_untargetable(target_id) != 0) {
                        continue;
                    }
                    if (action->targeting_type == TARGET_TYPE_UNIT
                        && ai->unit_had_move_opportunity[action->target_id] != 0) {
                        if (action->target_id != target_id) {
                            continue;
                        }
                    } else if (!(behavior->flags_1 & AI_ABILITY_TARGET_ALLIES)
                        && ai->unit_had_move_opportunity[target_id] != 0) {
                        continue;
                    }
                }
                action_apply_target_result(target_id);
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
        action_apply_attacker_result(action->actor_id);
    } while (work.continue_attack != 0);
    if (ai->hit_counter != 0xff && ai->ranked_origin_scan != 0) {
        if (ai->ability_effect_on_self == AI_ABILITY_EFFECT_USEFUL && self_hit != 0) {
            ai->consume_aim_tile = 1;
        } else if (ai->ability_effect_on_self == AI_ABILITY_EFFECT_ADVERSE && self_hit == 0) {
            ai->consume_aim_tile = 1;
        }
    }
    action_store_actor_data(&g_unit_stats[action->actor_id]);
    if (reaction_mode == 0) {
        /* Reuse the finished loop counter's low 16 bits for the reaction ID. */
        while ((target_id = reaction_prepare_next((s16*)&target_index)) != -1) {
            if (ai->unit_had_move_opportunity[target_id] == 0) {
                ai_simulate_strikes(&g_unit_stats[target_id].action_command, 1);
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
static void ai_simulate_save_unit_state(ai_snapshot_mode_e mode) {
    ai_data_t* ai = &g_ai_data;
    ai_status_snapshot_t* status;
    ai_extended_snapshot_t* snapshot;
    unit_stats_t* unit;
    s32 i;
    s32 id;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        id = ai->unit_compact_indices[i];
        if (id == 0xff) {
            continue;
        }
        unit = &g_unit_stats[i];
        if (mode != AI_SNAPSHOT_BASELINE) {
            status = &ai->unit_status_records[id];
        } else {
            status = &g_ai_unit_snapshots[id].status;
        }
        status->active_battle_id = unit->active_battle_id;
        status->death_counter = unit->death_counter;
        ai_mem_copy_bytes(status->current_status, unit->status_sets.current, sizeof(status->current_status));
        if (mode != AI_SNAPSHOT_BASELINE) {
            status->death_sentence_ct = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)];
            snapshot = &g_ai_unit_snapshots[id];
            unit->active_battle_id = snapshot->status.active_battle_id;
            unit->death_counter = snapshot->status.death_counter;
            unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)]
                = snapshot->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)];
            ai_mem_copy_bytes(
                unit->status_sets.current, snapshot->status.current_status, sizeof(snapshot->status.current_status));
        } else {
            snapshot = (ai_extended_snapshot_t*)status;
            snapshot->team_flags = unit->team_flags;
            ai_mem_copy_bytes(snapshot->status_ct, unit->status_ct, STATUS_TIMED_COUNT);
            ai_mem_copy_bytes(snapshot->equipment, unit->equipment, sizeof(snapshot->equipment));
            snapshot->brave = unit->brave;
            ai_mem_copy_bytes(&snapshot->faith, &unit->faith, 4);
            snapshot->mp = unit->mp;
            ai_mem_copy_bytes(snapshot->base_attributes, unit->base_attributes, 3);
            ai_mem_copy_bytes(snapshot->attributes, unit->attributes, 4);
            ai_mem_copy_bytes(&snapshot->x, &unit->x, 3);
            snapshot->charged_ability_ct = unit->charged_ability_ct;
            ai_mem_copy_bytes(&snapshot->has_turn, &unit->has_turn, 3);
            snapshot->mount_info = unit->mount_info;
            ai_mem_copy_bytes(&snapshot->auto_battle_setting, &unit->auto_battle_setting, 8);
            ai_mem_copy_u16_values(&ai->unit_action_records[id], &unit->action_command, 20);
        }
    }
    if (mode == AI_SNAPSHOT_BASELINE) {
        ai_mem_copy_bytes(ai->team_golem, g_action_team_golem, sizeof(ai->team_golem));
    }
}

/*
 * Restore baseline unit data or suspended simulation status.
 *
 * BASELINE restores selected baseline fields, action records, and team Golem
 * values. Any non-baseline mode restores only the compact status record.
 */
static void ai_simulate_restore_unit_state(ai_snapshot_mode_e mode) {
    ai_data_t* ai = &g_ai_data;
    ai_status_snapshot_t* status;
    ai_extended_snapshot_t* snapshot;
    unit_stats_t* unit;
    s32 i;
    s32 id;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        id = ai->unit_compact_indices[i];
        if (id == 0xff) {
            continue;
        }
        unit = &g_unit_stats[i];
        if (mode != AI_SNAPSHOT_BASELINE) {
            status = &ai->unit_status_records[id];
        } else {
            status = &g_ai_unit_snapshots[id].status;
        }
        unit->active_battle_id = status->active_battle_id;
        unit->death_counter = status->death_counter;
        ai_mem_copy_bytes(unit->status_sets.current, status->current_status, sizeof(status->current_status));
        if (mode != AI_SNAPSHOT_BASELINE) {
            unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] = status->death_sentence_ct;
        } else {
            snapshot = (ai_extended_snapshot_t*)status;
            unit->team_flags = snapshot->team_flags;
            ai_mem_copy_bytes(unit->status_ct, snapshot->status_ct, STATUS_TIMED_COUNT);
            ai_mem_copy_bytes(unit->equipment, snapshot->equipment, sizeof(snapshot->equipment));
            unit->brave = snapshot->brave;
            ai_mem_copy_bytes(&unit->faith, &snapshot->faith, 4);
            unit->mp = snapshot->mp;
            ai_mem_copy_bytes(unit->base_attributes, snapshot->base_attributes, 3);
            ai_mem_copy_bytes(unit->attributes, snapshot->attributes, 4);
            ai_mem_copy_bytes(&unit->x, &snapshot->x, 3);
            unit->charged_ability_ct = snapshot->charged_ability_ct;
            ai_mem_copy_bytes(&unit->has_turn, &snapshot->has_turn, 3);
            unit->mount_info = snapshot->mount_info;
            ai_mem_copy_bytes(&unit->auto_battle_setting, &snapshot->auto_battle_setting, 8);
            ai_mem_copy_u16_values(&unit->action_command, &ai->unit_action_records[id], 20);
        }
    }
    if (mode == AI_SNAPSHOT_BASELINE) {
        ai_mem_copy_bytes(g_action_team_golem, ai->team_golem, sizeof(ai->team_golem));
    }
}

/* Keep in the target list only the units the considered ability is useful on.
 * Returns 1 while one is left. */
static s32 ai_target_filter_useful(void) {
    ai_data_t* ai;
    s32 result = 0;
    s32 i;

    ai = &g_ai_data;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        if (ai->targetability.live.unit_targetable[i] != 0) {
            if (ai_target_classify_ability_effect(i, 1) == AI_ABILITY_EFFECT_USEFUL) {
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
ai_ability_effect_e ai_target_classify_ability_effect(s32 unit_id, s32 check_reflect) {
    ai_data_t* ai = &g_ai_data;
    unit_stats_t* unit = &g_unit_stats[unit_id];
    ai_unit_decision_t* decision = &ai->unit_decisions[unit_id];
    s32 ct = g_ai_data.considered_ability.ct;
    s32 i;
    s32 bit;
    s32 mask;
    s32 status;
    s16 ability_id;

    /* Targets the ability cannot affect at all, as of when it resolves ct ticks from now. */
    if ((g_ai_data.considered_ability.ai_flags.word & AI_ABILITY_ENEMIES_ONLY) && !decision->enemy_flag)
        return AI_ABILITY_EFFECT_NONE;
    if ((ai->considered_ability.ai_flags.word & AI_ABILITY_ALLIES_ONLY) && decision->enemy_flag)
        return AI_ABILITY_EFFECT_NONE;
    if (unit_id == ai->acting_unit_id) {
        ability_id = ai->considered_ability.ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST
            && (g_ability_range_data[ability_id].flags_2 & ABILITY_SECONDARY_FLAG_2_CANNOT_HIT_CASTER))
            return AI_ABILITY_EFFECT_NONE;
    }
    if (check_reflect && (ai->considered_ability.ai_flags.word & AI_ABILITY_REFLECTABLE)
        && ai_status_is_active_through_delay(ct, unit, STATUS_ID_REFLECT))
        return AI_ABILITY_EFFECT_NONE;
    if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTED_BY_FAITH)
        && ai_status_is_active_through_delay(ct, unit, STATUS_ID_INNOCENT))
        return AI_ABILITY_EFFECT_NONE;
    if (ai_status_is_active_through_delay(ct, unit, STATUS_ID_JUMP))
        return AI_ABILITY_EFFECT_NONE;

    /* Death/revival timing determines which effects can still matter. */
    /* i flags a revive: the ability cancels Dead. */
    i = 0;
    if ((ai->considered_ability.ai_flags.word & AI_ABILITY_CANCEL_STATUS)
        && (ai->considered_ability.parameters.fields.status_infliction[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
            & STATUS_BYTE_MASK(STATUS_ID_DEAD)))
        i = 1;
    if (decision->targeting_flags_2 & AI_TARGET_DEAD_WITH_RERAISE) {
        s32 ability_ct = ai->considered_ability.ct;
        if (ability_ct < ai_calculate_clockticks_until_unit_acts(unit))
            return AI_ABILITY_EFFECT_NONE;
        if (i)
            return AI_ABILITY_EFFECT_NONE;
    }
    if (decision->targeting_flags_2 & AI_TARGET_DEAD_WITHOUT_RERAISE) {
        s32 ability_ct = ai->considered_ability.ct;
        if (ai_calculate_clockticks_until_death_counter_expires(unit) < ability_ct)
            return AI_ABILITY_EFFECT_NONE;
        if (!i)
            return AI_ABILITY_EFFECT_NONE;
    }

    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
        && (!(ai->considered_ability.ai_flags.word & AI_ABILITY_CANCEL_STATUS)
            || !(ai->considered_ability.parameters.fields.status_infliction[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
                & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))))
        return AI_ABILITY_EFFECT_NONE;
    if (ai_status_is_active_through_delay(ct, unit, STATUS_ID_WALL)
        && (!(ai->considered_ability.ai_flags.word & AI_ABILITY_CANCEL_STATUS)
            || !(ai->considered_ability.parameters.fields.status_infliction[STATUS_BYTE_INDEX(STATUS_ID_WALL)]
                & STATUS_BYTE_MASK(STATUS_ID_WALL))))
        return AI_ABILITY_EFFECT_NONE;

    i = ai->considered_ability.element;
    /* i now holds the element: nullified elements and Earth against a floating unit do nothing. */
    if (i) {
        if (i == (i & unit->elemental_affinity[ELEMENTAL_AFFINITY_NULLIFY])
            || (i == ELEMENT_EARTH
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
            /* i now flags a Charm that lasts until the ability resolves and whose removal matters. */
            if (ai_status_is_active_through_delay(ct, unit, STATUS_ID_CHARM)
                && ai_status_can_cancellation_affect_turn(unit, STATUS_ID_CHARM))
                i = 1;
            else
                i = 0;
            if ((ai->considered_ability.ai_flags.word & AI_ABILITY_AFFECTS_HP)
                && g_command_menu_types_by_skillset[ai->considered_ability.skillset] != ACTION_MENU_TYPE_CHARGE) {
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
                    & STATUS_BYTE_MASK(STATUS_ID_CONFUSION))
                    return AI_ABILITY_EFFECT_USEFUL;
                if (ai_status_is_active_through_delay(ct, unit, STATUS_ID_SLEEP))
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

    /* Adding status is useful when some status is new to the target, not blocked by
     * immunity, and worth it for the target's condition. */
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

    /* Cancelling status is useful when the target has a non-innate status worth removing. */
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_CANCEL_STATUS) {
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            mask = ai->considered_ability.parameters.fields.status_infliction[i];
            mask &= unit->status_sets.current[i];
            if (mask) {
                mask ^= mask & unit->status_sets.innate[i];
                if (mask) {
                    /* Bytes 3 and 4 hold the timed statuses: drop those that end before the ability
                     * resolves or whose removal would not affect the target's turn. */
                    if (i >= 3) {
                        for (bit = 0; bit < 8; bit++) {
                            s32 flag = 0x80 >> bit;
                            if (mask & flag) {
                                status = i * 8 + bit;
                                if (!ai_status_is_active_through_delay(ct, unit, status)
                                    || !ai_status_can_cancellation_affect_turn(unit, status))
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

    /* A stat-changing ability is useful, except that Praise, Accumulate and Yell
     * count only on an even roll; an odd roll falls through to the Unequip test. */
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
s32 ai_status_is_active_through_delay(s32 delay, const unit_stats_t* unit, s32 status_id) {
    s32 byte_off = status_id / 8;

    if (!(unit->status_sets.current[byte_off] & (0x80 >> (status_id - byte_off * 8)))) {
        return 0;
    }
    if (status_id == STATUS_ID_JUMP) {
        if (unit->charged_ability_ct < delay) {
            return 0;
        }
    } else {
        /* Statuses before Poison have no countdown. */
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
static s32 ai_status_can_cancellation_affect_turn(const unit_stats_t* unit, s32 status) {
    if (status == STATUS_ID_CHARM || (status - STATUS_ID_DONT_MOVE) < 2U) {
        if (ai_calculate_clockticks_until_unit_acts(unit) > unit->status_ct[STATUS_CT_INDEX(status)]) {
            return 0;
        }
    }
    return 1;
}

/* For an enemy-targeting ability, drop from the target list each enemy that
 * has a movement opportunity before the ability lands. Returns 1 while a target is left. */
static s32 ai_target_exclude_enemies_with_move_opportunity(void) {
    s32 result = 0;
    s32 i;
    ai_data_t* ai = &g_ai_data;
    u8* walk;

    /* The target walks a pointer that starts at the block base and steps by
     * sizeof(ai_unit_decision_t), reaching unit_decisions[i].enemy_flag as a
     * 0x1834 displacement; a pointer started at unit_decisions adds an addiu. */
    for (i = 0, walk = (u8*)ai; i < UNIT_SLOT_COUNT; i++, walk += sizeof(ai_unit_decision_t)) {
        if (ai->targetability.live.unit_targetable[i] != 0) {
            if ((ai->considered_ability.ai_flags.word & AI_ABILITY_TARGET_ENEMIES)
                && (walk[offsetof(ai_data_t, unit_decisions[0].enemy_flag)] != 0)
                && (ai_can_unit_move_by_charge_deadline(i) != 0)) {
                ai->targetability.live.unit_targetable[i] = 0;
            } else {
                result = 1;
            }
        }
    }
    return result;
}

/* 1 when the unit gets a turn by the considered ability's charge deadline
 * and is eligible to move by then; 0 for an instant ability. */
static s32 ai_can_unit_move_by_charge_deadline(s32 unit_index) {
    s32 min_ct;
    u8 ct;

    ct = g_ai_data.considered_ability.ct;
    if (ct != 0) {
        min_ct = ai_calculate_clockticks_until_unit_acts(&g_unit_stats[unit_index]);
        if (min_ct != 0x7FFFFFFF) {
            if (ct >= min_ct) {
                return ai_is_unit_movement_eligible_after_delay(ct, unit_index);
            }
        }
    }
    return 0;
}

/* 1 when the unit is eligible to move limit clockticks from now: present, Speed above 0,
 * not crystal, petrified or treasure, with no Stop, Don't Move or Sleep that
 * lasts that long, and alive, or dead with Reraise and due a turn by then. */
static s32 ai_is_unit_movement_eligible_after_delay(s32 limit, s32 unit_id) {
    unit_stats_t* unit = &g_unit_stats[unit_id];

    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
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
    if (ai_status_is_active_through_delay(limit, unit, STATUS_ID_STOP) != 0) {
        return 0;
    }
    if (ai_status_is_active_through_delay(limit, unit, STATUS_ID_DONT_MOVE) != 0) {
        return 0;
    }
    if (ai_status_is_active_through_delay(limit, unit, STATUS_ID_SLEEP) != 0) {
        return 0;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) == 0) {
        return 1;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)] & STATUS_BYTE_MASK(STATUS_ID_RERAISE)) == 0
        || limit < ai_calculate_clockticks_until_unit_acts(unit)) {
        return 0;
    }
    return 1;
}

/* Place the acting unit's own record at the supplied AI coordinates; the AI passes
 * acting_unit_coords to put it back. */
void ai_place_acting_unit(const ai_coords_t* coord) {
    g_ai_data.acting_unit->x = coord->fields.x;
    g_ai_data.acting_unit->position.y = coord->fields.y;
    g_ai_data.acting_unit->position.higher_elevation = coord->fields.layer;
}

/* Pack a unit's tile coordinates and terrain-layer bit (position bit 15). */
void ai_copy_unit_coordinates(s32 unit_idx, ai_coords_t* dst) {
    dst->fields.x = g_unit_stats[unit_idx].x;
    dst->fields.y = g_unit_stats[unit_idx].position.y;
    dst->fields.layer = g_unit_stats[unit_idx].position.higher_elevation;
    dst->fields.zero = 0;
}
