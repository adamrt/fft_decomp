/* Choose AI movement, actions, targets and facing for each automatic-battle policy. */

#include "fft/ability.h"
#include "fft/ai.h"
#include "fft/ai_decision.h"
#include "fft/battle_map.h"
#include "fft/command.h"
#include "fft/item.h"
#include "fft/move.h"
#include "fft/skillset.h"
#include "fft/status.h"
#include "fft/target.h"
#include "fft/turn_command.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/types.h"

typedef enum unit_filter {
    AI_UNIT_FILTER_ANY = 0,
    AI_UNIT_FILTER_ENEMIES = 1,
    AI_UNIT_FILTER_ALLIES = 2,
    AI_UNIT_FILTER_SPECIFIC = 3,
    AI_UNIT_FILTER_STATUS = 4
} unit_filter_e;

typedef enum destination_order {
    AI_DESTINATION_PRIORITY_FIRST = 0,
    AI_DESTINATION_DISTANCE_FIRST = 1
} destination_order_e;

typedef enum rank_order { AI_RANK_BELOW = 0, AI_RANK_EQUAL = 1, AI_RANK_ABOVE = 2 } rank_order_e;

static turn_command_t g_ai_command_history[2];

static s32 scenarios_build(void);

static s32 targeted_build(void);

static s32 origin_ability_range_calculate(void);

static s32 status_can_cancel_current(const unit_stats_t* unit, s32 status_bit);

static s32 target_is_out_of_play(s32 unit_id);

static rank_order_e rank_compare(const ai_action_rank_t* first, const ai_action_rank_t* second);

static void status_only_ability_disable(ai_ability_entry_t* entry, s32 status_id);

static s32 ability_list_evaluate(void);

static s32 status_cancellation_evaluate(s32 unit_id, s32 status_id);

static void foe_proximity_grid_fill(void);

static s32 target_distance_grid_fill(const ai_coords_t* target);

static s32 target_distance_tiles_filter(s32 distance_limit);

static void decision_target_focus(s32 unit_id);

static void acting_unit_init(void);

static void decision_target_consideration_init(void);

static void rank_baseline_seed(void);

static s32 decision_flags_refresh(void);

static s32 retreat_build(void);

static s32 reachable_tiles_score(void);

static s32 autobattle_select(void);

static s32 save_fading_life_select(void);

static void fallback_select(void);

static s32 general_select(void);

static s32 action_select(void);

static s32 random_select(void);

static s32 ranked_select(void);

static s32 protect_allies_select(void);

static s32 retreat_select(void);

static s32 status_imposed_select(void);

static s32 fight_for_life_select(void);

static s32 target_active_units_select(unit_filter_e mode, s32 target_id);

static s32 destination_select(s32 max_distance, destination_order_e order);

static void destination_with_nearest_fallback_select(s32 range);

static void destination_with_range_floor_select(s32 distance_limit);

static s32 target_in_peril_select(void);

static void ability_considerations_set(s32 action_taken);

static void candidate_actor_reachability_set(void);

static s32 reachable_tiles_build(s32 movement_taken);

static s32 target_is_excluded_from_imminent_enemies(s32 unit_id);

static s32 random_tile_take(
    u16 tiles[BATTLE_MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT], ai_coords_t* coordinates, s32 count);

static void decision_team_targeting_flags_update(void);

/* Build the acting AI unit's next command (move, act or end turn), deciding a new
 * action first when none is pending. Returns -1 when the frame's scanline budget runs
 * out, resuming at the suspended stage on the next call, else 0.
 *
 * A new action is decided when none is, and again once the unit has acted (to
 * choose where to move afterward). Each call then returns one step of it: the
 * move to the action's tile, unless the unit has moved or is still to act
 * before moving; else the act; else END_TURN once the unit has acted or the
 * action has no skillset. When the previous command changed neither the moved
 * nor the acted flag, the turn ends rather than repeat it. The turn also ends
 * at once when the acting unit, or any present unit, has no compact AI record. */
s32 ai_command_build(s32 unit_id, turn_command_t* command) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 i;
    s32 ticks = VSync(1);

    if (ticks >= AI_DECISION_SCANLINE_LIMIT)
        return -1;
    if (g_ai_workspace_storage.resume_pending) {
        /* Resume: re-enter the suspended decision stage. */
        switch (g_ai_workspace_storage.main_ai_state) {
        case AI_COMMAND_BUILD_REFRESH:
            goto refresh;
        case AI_COMMAND_BUILD_MOVEMENT:
            goto movement;
        case AI_COMMAND_BUILD_CHOOSE_ACTION:
            goto choose;
        }
    }
    if (ai->selected_action.rank.decided == AI_ACTION_DECIDED && ai->acting_unit->movement_taken == ai->movement_taken
        && ai->acting_unit->action_taken == ai->action_taken) {
        command->kind = TURN_COMMAND_END_TURN;
        return 0;
    }
    ai->acting_unit_id = unit_id;
    if (ai->unit_compact_indices[unit_id] >= AI_COMPACT_UNIT_COUNT) {
        command->kind = TURN_COMMAND_END_TURN;
        return 0;
    }
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        if (g_unit_stats[i].active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE
            && ai->unit_compact_indices[i] >= AI_COMPACT_UNIT_COUNT) {
            command->kind = TURN_COMMAND_END_TURN;
            return 0;
        }
    }
    acting_unit_init();
    if (ai->acting_unit->charged_ability_ct == 0xff)
        ai->acting_unit_decision->targeting_flags_1 &= ~AI_TARGETING_FLAG_1_REFLECTED_ACTION;
    if (!ai->selected_action.rank.decided || ai->acting_unit->action_taken) {
        ai_simulation_begin();
    refresh:
        if (decision_flags_refresh() == -1) {
            ai->main_ai_state = AI_COMMAND_BUILD_REFRESH;
            ai->resume_pending = 1;
            return -1;
        }
        ability_considerations_set(ai->acting_unit->action_taken);
    movement:
        if (reachable_tiles_build(ai->acting_unit->movement_taken) == -1) {
            ai->main_ai_state = AI_COMMAND_BUILD_MOVEMENT;
            ai->resume_pending = 1;
            return -1;
        }
        ai->resume_pending = 0;
        decision_team_targeting_flags_update();
        decision_target_consideration_init();
    choose:
        if (action_select() == -1) {
            ai->main_ai_state = AI_COMMAND_BUILD_CHOOSE_ACTION;
            ai->resume_pending = 1;
            return -1;
        }
        ai->resume_pending = 0;
        ai->selected_action.rank.decided = AI_ACTION_DECIDED;
        if (ai->selected_action.reflected_action)
            ai->acting_unit_decision->targeting_flags_1 |= AI_TARGETING_FLAG_1_REFLECTED_ACTION;
        ai_simulation_end();
    }
    if (!ai->acting_unit->movement_taken && (ai->acting_unit->action_taken || !ai->selected_action.act_before_move)) {
        ai->selected_action.coords.fields.zero = 0;
        if (ai->selected_action.coords.word != ai->acting_unit_coords.word) {
            command->kind = TURN_COMMAND_MOVE;
            command->data.move.x = ai->selected_action.coords.fields.x;
            command->data.move.y = ai->selected_action.coords.fields.y;
            command->data.move.layer = ai->selected_action.coords.fields.layer;
            goto preserve;
        }
    }
    if (ai->acting_unit->action_taken || !ai->selected_action.command.skillset_id) {
        command->kind = TURN_COMMAND_END_TURN;
    } else {
        command->kind = TURN_COMMAND_ACT;
        ai_mem_copy_u16_values(&command->data.action, &ai->selected_action.command, sizeof(command->data.action));
    }
preserve:
    ai->movement_taken = ai->acting_unit->movement_taken;
    ai->action_taken = ai->acting_unit->action_taken;
    ai_acting_unit_place(&ai->acting_unit_coords);
    /* Shift the two-entry command history: the previous command moves to entry 0
     * and this one becomes entry 1. */
    ai_mem_copy_u16_values(&g_ai_command_history[0], &g_ai_command_history[1], sizeof(turn_command_t));
    ai_mem_copy_u16_values(&g_ai_command_history[1], command, sizeof(turn_command_t));
    return 0;
}

/*
 * Populate the AI block acting-unit fields for the unit whose turn is being prepared.
 *
 * A mounted rider uses its mount's movement allowance.
 */
static void acting_unit_init(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* unit;
    unit_stats_t* acting;
    s32 mount_info;
    s32 id = g_ai_workspace_storage.acting_unit_id;

    unit = &g_unit_stats[id];
    g_ai_workspace_storage.acting_unit = unit;
    g_ai_workspace_storage.acting_unit_decision = &ai->unit_decisions[id];
    g_ai_workspace_storage.acting_unit_coords.fields.x = unit->x;
    g_ai_workspace_storage.acting_unit_coords.fields.y = unit->position.y;
    g_ai_workspace_storage.acting_unit_coords.fields.layer = unit->position.higher_elevation;
    g_ai_workspace_storage.acting_unit_coords.fields.zero = 0;
    g_ai_workspace_storage.acting_unit_compact_index = ai->unit_compact_indices[id];
    g_ai_workspace_storage.acting_unit_team = unit->team_flags & UNIT_TEAM_MASK;
    g_ai_workspace_storage.acting_unit_remaining_clockticks = ai_unit_turn_delay_calculate(unit);
    acting = g_ai_workspace_storage.acting_unit;
    mount_info = acting->mount_info;
    if (mount_info & UNIT_MOUNT_INFO_FLAG_RIDER) {
        g_ai_workspace_storage.acting_unit_move = g_unit_stats[mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK].move;
    } else {
        g_ai_workspace_storage.acting_unit_move = acting->move;
    }
    ai->current_action.coords.fields.zero = 0;
    ai->current_action.command.actor_id = ai->acting_unit_id;
}

/*
 * Rebuild every unit's per-turn decision flags, then run the refresh pass of
 * the action simulation.
 *
 * Each in-play unit gets its enemy flag (against the acting unit's team), its
 * dead, HP and MP target flags and its considered flag; an untargetable unit
 * only gains AI_TARGET_UNTARGETABLE. The simulation then marks the transient
 * statuses each unit will have (ai_action_simulate_and_score). Its
 * REFRESH_UNIT_STATUS_FLAGS setting stays on across a suspension and is
 * cleared once the pass completes. Returns -1 to suspend, else 0.
 */
static s32 decision_flags_refresh(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* unit;
    ai_unit_decision_t* decision;
    s32 unit_id;

    /* A resumed decision goes straight back to the suspended simulation. */
    if (!g_ai_workspace_storage.resume_pending) {
        for (unit_id = 0; unit_id < UNIT_SLOT_COUNT; unit_id++) {
            unit = &g_unit_stats[unit_id];
            decision = &ai->unit_decisions[unit_id];
            if (ai_target_is_untargetable(unit_id)) {
                /* Other cached fields are intentionally untouched for excluded units. */
                decision->targeting_flags_2 |= AI_TARGET_UNTARGETABLE;
                continue;
            }
            if (unit_id != ai->acting_unit_id && (unit->initial_team_flags & UNIT_TEAM_MASK) != ai->acting_unit_team)
                decision->enemy_flag = 1;
            else
                decision->enemy_flag = 0;
            decision->targeting_flags_2 &= AI_TARGET_CONSERVE_CT | AI_TARGET_TEAM_SUPPORT_UNAVAILABLE;
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) {
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)]
                    & STATUS_BYTE_MASK(STATUS_ID_RERAISE))
                    decision->targeting_flags_2 |= AI_TARGET_DEAD_WITH_RERAISE;
                else
                    decision->targeting_flags_2 |= AI_TARGET_DEAD_WITHOUT_RERAISE;
            }
            if (unit->hp < unit->max_hp / 2)
                decision->targeting_flags_2 |= AI_TARGET_HP_BELOW_HALF;
            if (decision->mp_ability_mod) {
                if (unit->mp < unit->max_mp / 2)
                    decision->targeting_flags_2 |= AI_TARGET_MP_CONSTRAINED;
                if (ai->acting_unit_team && unit->mp < decision->highest_mp_cost)
                    decision->targeting_flags_2 |= AI_TARGET_MP_CONSTRAINED;
            }
            decision->targeting_flags_2 &= ~AI_TARGET_HP_BELOW_THREE_QUARTERS;
            /* Cross-multiply the strict three-quarter HP threshold. */
            if (unit->hp * 4 < unit->max_hp * 4 - unit->max_hp)
                decision->targeting_flags_2 |= AI_TARGET_HP_BELOW_THREE_QUARTERS;
            decision->flags &= ~AI_DECISION_CONSIDERED;
            if (!decision->enemy_flag || unit->charged_ability_ct != 0xff)
                decision->flags |= AI_DECISION_CONSIDERED;
        }
        ai->current_action.act_before_move = 1;
        ai->current_action.command.skillset_id = 0;
        ai->targetability.live.target_setting_flags |= AI_TARGET_SETTING_REFRESH_UNIT_STATUS_FLAGS;
    }
    if (ai_action_simulate_and_score() == -1) {
        return -1;
    }
    ai->targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_REFRESH_UNIT_STATUS_FLAGS;
    return 0;
}

/*
 * Update team-dependent AI targeting flags.
 *
 * Retreat support requires at least 2 eligible non-enemies, including one
 * neither Critical nor Frog. The unavailable-team-support flag propagates to
 * all cached non-enemies, not just the units admitted to those counts.
 */
static void decision_team_targeting_flags_update(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    ai_unit_decision_t* acting_decision = g_ai_workspace_storage.acting_unit_decision;
    s32 ally_count;
    s32 enemy_count;
    s32 impaired_ally_count;
    s32 i;

    g_ai_workspace_storage.single_enemy_remaining = 0;
    if ((acting_decision->targeting_flags_2 & AI_TARGET_TEAM_SUPPORT_UNAVAILABLE)
        || (g_ai_workspace_storage.acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
            & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE))) {
        g_ai_workspace_storage.targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE;
        return;
    }
    ally_count = 0;
    enemy_count = 0;
    impaired_ally_count = 0;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        if (target_is_out_of_play(i)) {
            continue;
        }
        if (ai->unit_decisions[i].enemy_flag) {
            enemy_count++;
        } else {
            ally_count++;
            if (g_unit_stats[i].status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
                & (STATUS_BYTE_MASK(STATUS_ID_FROG) | STATUS_BYTE_MASK(STATUS_ID_CRITICAL))) {
                impaired_ally_count++;
            }
        }
    }
    if (ally_count == impaired_ally_count || ally_count < 2) {
        if (ai->acting_unit_team) {
            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                if (!ai->unit_decisions[i].enemy_flag) {
                    ai->unit_decisions[i].targeting_flags_2 |= AI_TARGET_TEAM_SUPPORT_UNAVAILABLE;
                }
            }
        }
        ai->targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE;
    } else {
        if (enemy_count == 1) {
            ai->single_enemy_remaining = 1;
        }
        ai->targetability.live.target_setting_flags |= AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE;
    }
}

/*
 * Prepare the acting unit's automatic targeting policy and focused target.
 *
 * Player-team units under direct control retain their configured automatic
 * behavior. Other units may abandon special focus behavior when its target is
 * no longer eligible, then select an appropriate nearby unit.
 */
static void decision_target_consideration_init(void) {
    ai_unit_decision_t* decision;
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* acting;
    s32 flags;
    u8 targeting_flags;
    ai_nearest_target_mode_e mode;
    s32 target;

    decision = g_ai_workspace_storage.acting_unit_decision;
    if (g_ai_workspace_storage.acting_unit_team == 0) {
        acting = g_ai_workspace_storage.acting_unit;
        if (acting->team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED) {
            ai->autobattle_setting = acting->auto_battle_setting;
            return;
        }
    }

    ai->autobattle_setting = ai->unit_autobattle_modes[ai->acting_unit_id];
    flags = decision->flags;
    if (flags & AI_DECISION_SPECIAL_BEHAVIOR) {
        if (!(ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INVITE)]
                & STATUS_BYTE_MASK(STATUS_ID_INVITE))) {
            if (decision->targeting_flags_2 & AI_TARGET_CONSERVE_CT)
                return;
            if (ai->autobattle_setting)
                return;
            if (flags & AI_DECISION_FOCUS_TARGET) {
                ai_unit_coordinates_copy(decision->main_target_id, &decision->target);
                target = target_is_out_of_play(decision->main_target_id);
            } else {
                target = ai->single_enemy_remaining;
            }
            if (target == 0)
                return;
        }
        decision->flags &= ~AI_DECISION_SPECIAL_BEHAVIOR;
    }

    if (ai->acting_unit_team == 0) {
        ai->autobattle_setting = AI_AUTOBATTLE_MANUAL;
        /* Below 45/128 of max HP (about 35%), head for the nearest healer or crystal. */
        if ((ai->acting_unit->hp << 7) / ai->acting_unit->max_hp < 45) {
            mode = AI_NEAREST_HEALER_OR_CRYSTAL;
        } else {
            for (target = 0; target < UNIT_SLOT_COUNT; target++)
                ai->targetability.live.unit_targetable[target] = 1;
            mode = AI_NEAREST_ELIGIBLE_ENEMY;
        }
        target = ai_target_find_nearest(mode);
        decision_target_focus(target);
    } else {
        targeting_flags = decision->targeting_flags_1;
        if (!(targeting_flags & AI_TARGETING_FLAG_1_MOSTLY_SUPPORT)
            || (decision->targeting_flags_2 & AI_TARGET_TEAM_SUPPORT_UNAVAILABLE)) {
            ai->autobattle_setting = AI_AUTOBATTLE_MANUAL;
            for (target = 0; target < UNIT_SLOT_COUNT; target++)
                ai->targetability.live.unit_targetable[target] = 1;
            mode = AI_NEAREST_ELIGIBLE_ENEMY;
            target = ai_target_find_nearest(mode);
            decision_target_focus(target);
        } else {
            ai->autobattle_setting = AI_AUTOBATTLE_FADING_LIFE;
        }
    }
}

/* Make the unit the acting unit's main target: switch its decision from
 * stay-near-coordinates to focus-target and take the unit's tile as the
 * decision's target coordinates. */
static void decision_target_focus(s32 unit_id) {
    ai_unit_decision_t* decision = g_ai_workspace_storage.acting_unit_decision;

    decision->main_target_id = unit_id;
    decision->flags = (decision->flags | AI_DECISION_FOCUS_TARGET) & ~AI_DECISION_STAY_NEAR_COORDINATES;
    ai_unit_coordinates_copy(unit_id, &decision->target);
}

/* Build the AI walkable and reachable tile rows for the acting unit.
 *
 * A fresh decision (resume_pending 0) sets the water penalty, rebuilds the
 * walkable rows, clears scenario 0 and spreads movement from the unit's tile
 * (a rider spreads with its mount's Move); a unit that has moved or has
 * Don't Move reaches only its own tile. Scenario 0 then takes every tile the
 * spread marked as a move destination (within Move by distance for a
 * teleporter), drops tiles other units stand on and, unless the unit has
 * Confusion or Berserk, the known trap tiles; the unit's own tile stays
 * reachable. Returns -1 when the frame budget is spent, to finish on the next
 * call. i and j are reused as the spread unit id and the teleport-range
 * distances, as the target's register assignment shows. */
static s32 reachable_tiles_build(s32 movement_taken) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* unit;
    battle_map_tile_t* tile;
    s32 level;
    s32 y;
    s32 x;
    s32 i;
    u8 mount_info;
    s32 j;
    ai_coords_t* coords;

    if (g_ai_workspace_storage.resume_pending == 0) {
        if ((g_ai_workspace_storage.acting_unit->ability_flags.movement[2]
                & (UNIT_MOVEMENT_SET_3_MOVE_IN_WATER | UNIT_MOVEMENT_SET_3_WALK_ON_WATER | UNIT_MOVEMENT_SET_3_FLOAT))
            || (g_ai_workspace_storage.acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
                & STATUS_BYTE_MASK(STATUS_ID_FLOAT))) {
            g_ai_workspace_storage.water_penalty = 0;
        } else {
            g_ai_workspace_storage.water_penalty = 1;
        }

        ai_mem_zero_32bit_values(ai->walkable_tiles, sizeof(ai->walkable_tiles));
        for (level = 0; level < BATTLE_MAP_TERRAIN_LAYER_COUNT; level++) {
            for (y = 0; y < ai->terrain_depth; y++) {
                for (x = 0; x < ai->terrain_width; x++) {
                    tile = &g_battle_map_tile_data[level * BATTLE_MAP_TILES_PER_LAYER + y * ai->terrain_width + x];
                    if (!(tile->flags.blocked) && (tile->surface.type) != BATTLE_MAP_SURFACE_CROSS_SECTION) {
                        /* Row bit 15 is x = 0. */
                        ai->walkable_tiles[level][y] |= 0x8000 >> x;
                    }
                }
            }
        }

        ai->movement_scenario = 0;
        for (j = 0; j < AI_GRID_ROW_COUNT; j++) {
            ai->reachable_tiles[ai->movement_scenario][0][j] = 0;
            ai->reachable_tiles[ai->movement_scenario][1][j] = 0;
        }
        if (movement_taken != 0
            || (ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)]
                & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE))) {
            ai->reachable_tiles[0][ai->acting_unit_coords.fields.layer][ai->acting_unit_coords.fields.y]
                = 0x8000 >> ai->acting_unit_coords.fields.x;
            return 0;
        }
        mount_info = ai->acting_unit->mount_info;
        if (mount_info & UNIT_MOUNT_INFO_FLAG_RIDER) {
            i = mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK;
        } else {
            i = ai->acting_unit_id;
        }
        move_range_build(
            i, ai->acting_unit_coords.fields.x, ai->acting_unit_coords.fields.y, ai->acting_unit_coords.fields.layer);
    }

    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        return -1;
    }
    ai->resume_pending = 0;
    for (level = 0; level < BATTLE_MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->terrain_depth; y++) {
            for (x = 0; x < ai->terrain_width; x++) {
                tile = &g_battle_map_tile_data[level * BATTLE_MAP_TILES_PER_LAYER + y * ai->terrain_width + x];
                if (ai->acting_unit->ability_flags.movement[1] & UNIT_MOVEMENT_SET_2_TELEPORT) {
                    if (!(ai->acting_unit->mount_info & UNIT_MOUNT_INFO_FLAG_RIDER)) {
                        i = ai->acting_unit_coords.fields.x - x;
                        if (i < 0) {
                            i = -i;
                        }
                        j = ai->acting_unit_coords.fields.y - y;
                        if (j < 0) {
                            j = -j;
                        }
                        if (ai->acting_unit_move < i + j) {
                            continue;
                        }
                    }
                }
                if (tile->ceiling_depth_and_marks.move_destination) {
                    ai->reachable_tiles[0][level][y] |= 0x8000 >> x;
                }
            }
        }
    }

    /* Other units' tiles are no destinations, except a crystal or treasure, which the unit
     * picks up by stepping on it. */
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE && i != ai->acting_unit_id
            && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
            && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
            ai->reachable_tiles[0][unit->position.higher_elevation][unit->position.y] &= ~(0x8000 >> unit->x);
        }
    }
    ai->reachable_tiles[0][ai->acting_unit_coords.fields.layer][ai->acting_unit_coords.fields.y]
        |= 0x8000 >> ai->acting_unit_coords.fields.x;

    if (*(u32*)&ai->acting_unit->status_sets.current[0]
        & (STATUS_PACKED_MASK(STATUS_ID_CONFUSION) | STATUS_PACKED_MASK(STATUS_ID_BERSERK))) {
        return 0;
    }
    for (i = 0; i < AI_KNOWN_TRAP_COUNT; i++) {
        coords = &ai->known_trap_coords[i];
        if (coords->fields.x == 0xff) {
            break;
        }
        ai->reachable_tiles[0][coords->fields.layer][coords->fields.y] &= ~(0x8000 >> coords->fields.x);
    }
    ai->reachable_tiles[0][ai->acting_unit_coords.fields.layer][ai->acting_unit_coords.fields.y]
        |= 0x8000 >> ai->acting_unit_coords.fields.x;
    return 0;
}

/* Mark each unit's usable abilities and derive the acting unit's AI hints.
 *
 * For every in-play unit, abilities blocked by Frog, Don't Act, Silence,
 * Innocent, MP, item stock or level are left unmarked; usable ones are marked
 * usable and feed the enemy-team counters and the per-unit helpful ratio.
 * The acting unit's list then drops buff-only abilities the enemies cannot
 * threaten, aggregates cancellable statuses, range limits and the random-use
 * threshold. action_taken clears the usable bits again.
 *
 * threshold holds first the status-check CT and later the enemy-count cutoff,
 * and throw_id doubles as the status byte index of the second pass; the shared
 * variables reproduce the target's register allocation. */
static void ability_considerations_set(s32 action_taken) {
    ai_workspace_t* ai;
    ai_considered_ability_t* ability;
    s32 delay;
    s32 enemy_harmful = 0;
    s32 enemy_magical = 0;
    s32 enemy_physical = 0;
    s32 enemy_reflectable = 0;
    s32 enemy_earth = 0;
    s32 unit_harmful;
    s32 unit_helpful;
    s32 lowest_ratio = 0xff;
    s32 lowest_unit = 0xff;
    s32 unit_id;
    s32 i;
    ai_unit_decision_t* decision;
    unit_stats_t* unit;
    ai_ability_entry_t* entry;
    s32 ability_id;
    u8 menu;
    s32 best_level;
    s32 throw_id;
    s32 type_index;
    s32 item_id;
    s32 item_type;
    s32 lacking;
    s32 level;
    s32 total;
    s32 ratio;
    s32 threshold;
    s32 usable;
    s32 random_use;
    s32 range;

    ai = &g_ai_workspace_storage;
    ability = &g_ai_workspace_storage.considered_ability;

    /* First pass: mark every in-play unit's usable abilities and count what the enemy team can do. */
    unit_id = 0;
    do {
        if (target_is_out_of_play(unit_id) == 0) {
            decision = &ai->unit_decisions[unit_id];
            unit = &g_unit_stats[unit_id];
            unit_harmful = 0;
            unit_helpful = 0;
            if (unit_id == g_ai_workspace_storage.acting_unit_id) {
                delay = 0;
            } else {
                delay = ai_unit_turn_delay_calculate(unit);
            }

            decision->targeting_flags_1
                &= ~(AI_TARGETING_FLAG_1_HAS_RANGED_ABILITY | AI_TARGETING_FLAG_1_CAN_HEAL_ALLIES);
            for (i = 0; i < AI_ABILITY_LIST_COUNT; i++) {
                entry = &ai->ability_lists[ai->unit_compact_indices[unit_id]][i];
                entry->usage.policy.usable = 0;
                if (entry->skillset == SKILLSET_ID_NONE) {
                    break;
                }
                ai_ability_entry_load(entry);
                ability_id = ability->ability_id;
                threshold = delay + ability->ct;

                /* A frog can use only Frog and Frog Attack, and only a frog can use Frog Attack. */
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
                    if (ability_id != ABILITY_ID_BLACK_MAGIC_FROG && ability_id != ABILITY_ID_FROG_ATTACK) {
                        continue;
                    }
                } else if (ability_id == ABILITY_ID_FROG_ATTACK) {
                    continue;
                }

                /* Don't Act is checked when the unit acts; Silence and Innocent when the
                 * ability resolves, delay + CT ticks from now. */
                if (ai_status_is_active_through_delay(delay, unit, STATUS_ID_DONT_ACT)) {
                    continue;
                }
                if ((ability->ai_flags.groups.target_effect_flags & AI_ABILITY_AFFECTED_BY_SILENCE)
                    && ai_status_is_active_through_delay(threshold, unit, STATUS_ID_SILENCE)) {
                    continue;
                }
                if ((ability->ai_flags.groups.target_effect_flags & AI_ABILITY_AFFECTED_BY_FAITH)
                    && ai_status_is_active_through_delay(threshold, unit, STATUS_ID_INNOCENT)) {
                    continue;
                }
                if (ability->mp_cost > unit->mp) {
                    continue;
                }

                /* Blue-team units need the item in the party inventory; other teams need only its level. */
                menu = g_command_menu_types_by_skillset[entry->skillset];
                if (menu == ACTION_MENU_TYPE_ITEM_INVENTORY || menu == ACTION_MENU_TYPE_KATANA_INVENTORY
                    || ability_id == ABILITY_ID_THROW_SHURIKEN || ability_id == ABILITY_ID_THROW_BALL) {
                    if (!(unit->initial_team_flags & UNIT_TEAM_MASK)) {
                        if (g_item_quantities[ability->parameters.fields.item_id] == 0) {
                            continue;
                        }
                    } else if (g_item_primary_data[ability->parameters.fields.item_id].required_level > unit->level) {
                        continue;
                    }
                } else if (menu == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
                    if (unit_id != g_ai_workspace_storage.acting_unit_id) {
                        continue;
                    }
                    /* Only the acting unit weighs Throw. Keep the highest-level item among its known
                     * throws: the blue team needs 5 in stock, other teams the item's level. Throw's
                     * learned bits 0 and 1 are Shuriken and Ball, so Knife is bit 2. */
                    best_level = -1;
                    for (throw_id = ABILITY_ID_THROW_KNIFE, type_index = 1; throw_id < ABILITY_ID_THROW_BALL;
                        throw_id++, type_index++) {
                        if (ai_ability_is_known(unit_id, ability->skillset, throw_id - (ABILITY_ID_THROW_KNIFE - 2))) {
                            item_type = g_ability_throw_item_types[type_index];
                            for (item_id = 0; item_id < ITEM_ID_COUNT; item_id++) {
                                if (g_item_primary_data[item_id].type == item_type) {
                                    level = g_item_primary_data[item_id].required_level;
                                    if (level >= best_level) {
                                        if (!(unit->initial_team_flags & UNIT_TEAM_MASK)) {
                                            lacking = g_item_quantities[item_id] < 5;
                                        } else {
                                            lacking = unit->level < level;
                                        }
                                        if (!lacking) {
                                            best_level = level;
                                            ai->throw_ability_id = throw_id;
                                            ai->throw_weapon_id = item_id;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (best_level == -1) {
                        continue;
                    }
                }

                entry->usage.policy.usable = 1;
                if (!entry->usage.policy.requires_monster_skill) {
                    if (decision->enemy_flag) {
                        if (ability->ai_flags.groups.target_effect_flags & AI_ABILITY_TARGET_ENEMIES) {
                            enemy_harmful++;
                            if (ability->ai_flags.groups.trajectory_usage_flags & (AI_ABILITY_MAGICAL_ATTACK >> 16)) {
                                enemy_magical++;
                            }
                            if (ability->ai_flags.groups.trajectory_usage_flags & (AI_ABILITY_PHYSICAL_ATTACK >> 16)) {
                                enemy_physical++;
                            }
                            if (ability->ai_flags.groups.target_effect_flags & AI_ABILITY_REFLECTABLE) {
                                enemy_reflectable++;
                            }
                            if (ability->element & ELEMENT_EARTH) {
                                enemy_earth++;
                            }
                        }
                    }
                    if ((ability->ai_flags.groups.target_effect_flags
                            & (AI_ABILITY_AFFECTS_HP | AI_ABILITY_TARGET_ALLIES))
                        == (AI_ABILITY_AFFECTS_HP | AI_ABILITY_TARGET_ALLIES)) {
                        decision->targeting_flags_1 |= AI_TARGETING_FLAG_1_CAN_HEAL_ALLIES;
                    }
                    if (ability->ai_flags.groups.trajectory_usage_flags
                        & ((AI_ABILITY_DIRECT_TRAJECTORY | AI_ABILITY_ARC_TRAJECTORY) >> 16)) {
                        decision->targeting_flags_1 |= AI_TARGETING_FLAG_1_HAS_RANGED_ABILITY;
                    }
                    if (ability->ai_flags.groups.target_effect_flags & AI_ABILITY_TARGET_ENEMIES) {
                        unit_harmful++;
                    }
                    if (ability->ai_flags.groups.target_effect_flags & AI_ABILITY_TARGET_ALLIES) {
                        unit_helpful++;
                    }
                }
            }

            if (decision->enemy_flag == 0) {
                total = unit_helpful + unit_harmful;
                if (total == 0) {
                    decision->targeting_flags_1 &= ~AI_TARGETING_FLAG_1_MOSTLY_SUPPORT;
                } else {
                    ratio = unit_helpful * AI_ABILITY_SHARE_SCALE / total;
                    if (ratio >= AI_SUPPORT_SHARE_THRESHOLD) {
                        decision->targeting_flags_1 |= AI_TARGETING_FLAG_1_MOSTLY_SUPPORT;
                    } else {
                        decision->targeting_flags_1 &= ~AI_TARGETING_FLAG_1_MOSTLY_SUPPORT;
                    }
                    if (ratio < lowest_ratio) {
                        lowest_unit = unit_id;
                        lowest_ratio = ratio;
                    }
                }
            }
        }
        unit_id++;
    } while (unit_id < UNIT_SLOT_COUNT);

    usable = 0;
    /* The ally with the lowest helpful share is never treated as mostly support. */
    if (lowest_unit != 0xff) {
        ai->unit_decisions[lowest_unit].targeting_flags_1 &= ~AI_TARGETING_FLAG_1_MOSTLY_SUPPORT;
    }

    random_use = 0;
    decision = g_ai_workspace_storage.acting_unit_decision;
    ai->has_mp_recovery_ability = 0;
    ai->targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_HAS_HOLY_WATER;
    decision->ally_ability_min_reach = 0xff;
    decision->enemy_ability_min_reach = 0xff;
    threshold = enemy_harmful / 8;
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        ai->status_to_cancel[i] = 0;
    }

    /* Second pass, over the acting unit's own list. */
    for (i = 0; i < AI_ABILITY_LIST_COUNT; i++) {
        entry = &ai->ability_lists[g_ai_workspace_storage.acting_unit_compact_index][i];
        if (entry->skillset == SKILLSET_ID_NONE) {
            break;
        }
        ai_ability_entry_load(entry);
        if (entry->usage.policy.usable) {
            /* Drop buffs the enemies barely threaten: Shell, Protect or Reflect when at most an
             * eighth of their harmful abilities are magical, physical or reflectable; Float without Earth. */
            if (threshold >= enemy_magical) {
                status_only_ability_disable(entry, STATUS_ID_SHELL);
            }
            if (threshold >= enemy_physical) {
                status_only_ability_disable(entry, STATUS_ID_PROTECT);
            }
            if (threshold >= enemy_reflectable) {
                status_only_ability_disable(entry, STATUS_ID_REFLECT);
            }
            if (enemy_earth <= 0) {
                status_only_ability_disable(entry, STATUS_ID_FLOAT);
            }

            /* Test again: the checks above may have just cleared the bit. */
            if (entry->usage.policy.usable) {
                usable++;
                if (ability->ai_flags.groups.target_effect_flags & AI_ABILITY_RANDOM_USE) {
                    random_use++;
                }
                if (ability->ability_id == ABILITY_ID_ITEM_HOLY_WATER) {
                    ai->targetability.live.target_setting_flags |= AI_TARGET_SETTING_HAS_HOLY_WATER;
                }
                if (ability->ai_flags.groups.target_effect_flags & AI_ABILITY_CANCEL_STATUS) {
                    for (throw_id = 0; throw_id < STATUS_BYTE_COUNT; throw_id++) {
                        ai->status_to_cancel[throw_id] |= ability->parameters.fields.status_infliction[throw_id];
                    }
                }
                if ((ability->ai_flags.groups.target_effect_flags & (AI_ABILITY_AFFECTS_MP | AI_ABILITY_TARGET_ENEMIES))
                    == AI_ABILITY_AFFECTS_MP) {
                    ai->has_mp_recovery_ability = 1;
                }

                range = ability->parameters.fields.range + ability->parameters.fields.aoe;
                if ((ability->ai_flags.groups.target_effect_flags & AI_ABILITY_TARGET_ENEMIES)
                    && range < decision->enemy_ability_min_reach) {
                    decision->enemy_ability_min_reach = range;
                }
                if (ability->ai_flags.groups.target_effect_flags & AI_ABILITY_TARGET_ALLIES) {
                    if (g_command_menu_types_by_skillset[ability->skillset] == ACTION_MENU_TYPE_ITEM_INVENTORY) {
                        range = (range + 1) / 2;
                    }
                    if (range < decision->ally_ability_min_reach) {
                        decision->ally_ability_min_reach = range;
                    }
                }

                if (entry->usage.policy.force_consume_evaluated_aim_tiles) {
                    entry->usage.policy.consume_evaluated_aim_tiles = 1;
                } else if (!entry->usage.policy.exhaustive_origin_scan) {
                    if (ability->ct >= g_ai_workspace_storage.acting_unit_remaining_clockticks
                        || ai_target_classify_ability_effect(g_ai_workspace_storage.acting_unit_id, 1)
                            == AI_ABILITY_EFFECT_NONE) {
                        entry->usage.policy.consume_evaluated_aim_tiles = 1;
                    } else {
                        entry->usage.policy.consume_evaluated_aim_tiles = 0;
                    }
                }

                if (action_taken != 0) {
                    entry->usage.policy.usable = 0;
                }
            }
        }
    }

    /* Both reaches add the unit's Move; with no ally ability, ally reach is Move alone. */
    if (decision->enemy_ability_min_reach != 0xff) {
        decision->enemy_ability_min_reach += g_ai_workspace_storage.acting_unit_move;
    }
    if (decision->ally_ability_min_reach == 0xff) {
        decision->ally_ability_min_reach = 0;
    }
    decision->ally_ability_min_reach += g_ai_workspace_storage.acting_unit_move;
    if (usable != 0) {
        ai->random_use_threshold = random_use * AI_ABILITY_SHARE_SCALE / usable;
    }
}

/*
 * Disable an AI ability that does nothing except add one unwanted status.
 *
 * The 16-bit value update spans skillset and usage_flags so GCC emits the target's
 * single lhu/andi/sh sequence while clearing only the usable bit. A direct
 * status_infliction pointer folds +7 into its base; a workspace-relative byte
 * cursor retains the target's base pointer.
 */
static void status_only_ability_disable(ai_ability_entry_t* entry, s32 status_id) {
    ai_workspace_t* ai = &g_ai_workspace_storage;

    if ((ai->considered_ability.ai_flags.word & AI_ABILITY_ADD_STATUS) == 0) {
        return;
    }
    /* Only ally targeting and status addition may remain in this flag byte. */
    if ((ai->considered_ability.ai_flags.fields.flags_1
            & (AI_ABILITY_TARGET_ENEMIES | AI_ABILITY_UNEQUIP | AI_ABILITY_AFFECTS_STATS | AI_ABILITY_CANCEL_STATUS
                | AI_ABILITY_AFFECTS_MP | AI_ABILITY_AFFECTS_HP))
        != 0) {
        return;
    }

    /* The inflicted statuses must be this one alone: its own byte may hold only
     * its bit (shift reaches the bit's position within that byte) and every
     * other byte must be empty. */
    {
        s32 status_set;
        s32 mask;
        u8* cursor;
        s32 shift;

        status_set = 0;
        mask = 0x80;
        cursor = (u8*)&g_ai_workspace_storage;
        shift = status_id;
        for (; status_set < STATUS_BYTE_COUNT; status_set++, shift -= 8) {
            if (status_set == status_id / 8) {
                if ((cursor[offsetof(ai_workspace_t, considered_ability.parameters.fields.status_infliction)]
                        & ((mask >> shift) ^ 0xFF))
                    != 0) {
                    return;
                }
                /* A separate increment per branch keeps the target's cursor
                 * base; one shared increment folds the field offset into it. */
                cursor++;
                continue;
            }
            if (cursor[offsetof(ai_workspace_t, considered_ability.parameters.fields.status_infliction)] != 0) {
                return;
            }
            cursor++;
        }
    }
    entry->usage.policy.usable = 0;
}

/*
 * Load an ability-list entry into the considered AI ability.
 *
 * The entry's packed id names the ability and the unit that owns it. Attack,
 * Charge and Jump take their AI flags from the owner's weapon summary (Attack
 * and Charge also its range and element; Charge and Jump add their CT, Jump its
 * own range), and Math Skill is weighed as inflicting every status on map
 * tiles. Other abilities read the ability tables for flags, CT, element, MP
 * cost (halved by Half of MP), range, area and statuses, with item, katana and
 * throw abilities substituting their item. The current action's calculator
 * fields are cleared.
 */
void ai_ability_entry_load(const ai_ability_entry_t* entry) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 ability_id, unit_id, menu, status_id;
    unit_stats_t* unit;
    ai_weapon_data_t* weapon;
    ability_secondary_data_t* secondary;
    s32 value;
    s32 speed;

    ai_mem_zero_32bit_values(&ai->considered_ability, sizeof(ai_considered_ability_t));
    ai_mem_zero_32bit_values(&ai->current_action.command.calculator_type, 4);

    ai->considered_ability.skillset = entry->skillset;
    ability_id = g_ai_workspace_storage.considered_ability.ability_id
        = entry->packed_id & AI_ABILITY_ENTRY_ABILITY_MASK;
    unit_id = entry->packed_id >> AI_ABILITY_ENTRY_UNIT_SHIFT;
    unit = &g_unit_stats[unit_id];
    menu = g_command_menu_types_by_skillset[entry->skillset];
    status_id = 0;

    /* Attack, Jump, Charge and Math Skill fill in the whole ability and return;
     * the other menus break to the ability-table load below. */
    switch (menu) {
    case ACTION_MENU_TYPE_ATTACK:
    attack:
        weapon = &ai->unit_weapon_data[ai->unit_compact_indices[unit_id]];
        ai->considered_ability.parameters.fields.range = weapon->fields.range;
        ai->considered_ability.element = weapon->fields.element;
    weapon_flags:
        ai->considered_ability.parameters.fields.item_id = weapon->fields.weapon_id;
        ai->considered_ability.ai_flags.fields.flags_1 = weapon->fields.flags_1;
        ai->considered_ability.ai_flags.fields.flags_2 = weapon->fields.flags_2;
        ai->considered_ability.ai_flags.fields.flags_3 = weapon->fields.flags_3;
        ai->considered_ability.ai_flags.fields.flags_4 = entry->usage.flags;
        if (menu == ACTION_MENU_TYPE_JUMP)
            ai->considered_ability.ai_flags.word
                = (ai->considered_ability.ai_flags.word | AI_ABILITY_TARGET_ENEMIES) & ~AI_ABILITY_TARGET_ALLIES;
        return;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        status_id = g_item_secondary_data[ability_id - ABILITY_ID_ITEM_FIRST].status_infliction_id;
        /* The AI uses Move for Throw Item's range; the real targeting uses 4. */
        if (unit->ability_flags.support[2] & UNIT_SUPPORT_SET_3_THROW_ITEM)
            ai->considered_ability.parameters.fields.range = unit->move;
        else
            ai->considered_ability.parameters.fields.range = 1;
        ai->considered_ability.parameters.fields.aoe = 0;
        ai->considered_ability.parameters.fields.item_id = g_item_ability_item_ids[ability_id - ABILITY_ID_ITEM_FIRST];
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        ai->considered_ability.parameters.fields.item_id
            = ability_id - (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
        break;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        if (ability_id >= ITEM_ID_SHURIKEN && ability_id <= ITEM_ID_YAGYU_DARKNESS) {
            ai->considered_ability.ability_id = ABILITY_ID_THROW_SHURIKEN;
            ai->considered_ability.parameters.fields.item_id = ability_id;
            ai->considered_ability.element = g_item_weapon_data[ability_id].element;
        } else if (ability_id >= ITEM_ID_FIRE_BALL && ability_id <= ITEM_ID_LIGHTNING_BALL) {
            ai->considered_ability.ability_id = ABILITY_ID_THROW_BALL;
            ai->considered_ability.parameters.fields.item_id = ability_id;
            ai->considered_ability.element = g_item_weapon_data[ability_id].element;
        } else {
            ai->considered_ability.ability_id = ai->throw_ability_id;
            ai->considered_ability.parameters.fields.item_id = ai->throw_weapon_id;
        }
        ai->considered_ability.parameters.fields.range = unit->move;
        ability_id = ai->considered_ability.ability_id;
        break;
    case ACTION_MENU_TYPE_JUMP:
        /* Jump lands after half the clockticks to the jumper's following turn; a unit
         * other than the actor first waits out the clockticks to its coming turn. */
        if (unit_id == ai->acting_unit_id) {
            speed = ai->acting_unit->attributes[UNIT_ATTRIBUTE_SPEED];
            ai->considered_ability.ct = ((UNIT_TURN_CT_THRESHOLD - ai->acting_unit->ct) / speed) / 2;
        } else {
            if (unit->ct > UNIT_TURN_CT_THRESHOLD) {
                ai->considered_ability.ct = 0;
                value = unit->ct - UNIT_TURN_CT_THRESHOLD;
            } else {
                ai->considered_ability.ct
                    = (UNIT_TURN_CT_THRESHOLD - unit->ct) / unit->attributes[UNIT_ATTRIBUTE_SPEED];
                value = unit->attributes[UNIT_ATTRIBUTE_SPEED]
                    - (UNIT_TURN_CT_THRESHOLD - unit->ct) % unit->attributes[UNIT_ATTRIBUTE_SPEED];
            }
            ai->considered_ability.ct
                += ((UNIT_TURN_CT_THRESHOLD - value) / unit->attributes[UNIT_ATTRIBUTE_SPEED]) / 2;
        }
        if (!ability_id)
            ai->considered_ability.parameters.fields.range = 1;
        else
            ai->considered_ability.parameters.fields.range
                = g_ability_jump_data[ability_id - ABILITY_ID_JUMP_FIRST].range;
        weapon = &ai->unit_weapon_data[ai->unit_compact_indices[unit_id]];
        goto weapon_flags;
    case ACTION_MENU_TYPE_CHARGE:
        ai->considered_ability.ct = g_ability_charge_data[ability_id - ABILITY_ID_CHARGE_FIRST].ct;
        /* Placing this case above ATTACK to fall through reorders the blocks. */
        goto attack;
    case ACTION_MENU_TYPE_MATH_SKILL: {
        u32 status_cursor;
        s32 included = 0xff;
        /* Math Skill is weighed as inflicting every status: all five bytes become 0xff. */
        value = 4;
        /* A direct status pointer folds the member offset into its base;
         * retain the field-derived base cursor for the exact five-byte fill. */
        status_cursor = (u32)ai + value;
        do {
            *(u8*)(status_cursor + offsetof(ai_workspace_t, considered_ability.parameters.fields.status_infliction))
                = included;
            value--;
            status_cursor--;
        } while (value >= 0);
        ai->considered_ability.ai_flags.fields.flags_1 = 0xfc;
        ai->considered_ability.ai_flags.fields.flags_4 = entry->usage.flags;
        ai->considered_ability.ai_flags.word |= AI_ABILITY_TARGET_MAP_TILES;
        return;
    }
    }

    ai_mem_copy_u16_values(&ai->considered_ability.ai_flags, &g_ability_data[ability_id].ai_flags, 4);
    if (menu == ACTION_MENU_TYPE_ITEM_INVENTORY || menu == ACTION_MENU_TYPE_WEAPON_INVENTORY)
        ai->considered_ability.ai_flags.word |= AI_ABILITY_DIRECT_TRAJECTORY;
    ai->considered_ability.ai_flags.fields.flags_4 = entry->usage.flags;

    if (ability_id < ABILITY_ID_ITEM_FIRST) {
        s32 ct;
        secondary = &g_ability_range_data[ability_id];
        ct = secondary->ct;
        status_id = secondary->status_infliction_id;
        ai->considered_ability.ct = ct;
        ai->considered_ability.element = secondary->element;
        ai->considered_ability.mp_cost = secondary->mp_cost;
        if (unit->ability_flags.support[1] & UNIT_SUPPORT_SET_2_HALF_MP)
            ai->considered_ability.mp_cost >>= 1;
        if (secondary->flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE) {
            ai->considered_ability.parameters.fields.range
                = ai->unit_weapon_data[ai->unit_compact_indices[unit_id]].fields.range;
            ai->considered_ability.parameters.fields.aoe = 0;
        } else {
            ai->considered_ability.parameters.fields.range = secondary->range;
            ai->considered_ability.parameters.fields.aoe = secondary->aoe;
        }
        /* Folding this test into the CT adjustment's condition adds instructions. */
        if (secondary->flags_3 & ABILITY_SECONDARY_FLAG_3_PERSEVERE)
            goto copy_status;
    }

    /* Retail tests Monster Skill (0x80) and Defend (0x40) here, where the
     * real CT uses Short Charge (0x08) and Non-Charge (0x04); see QUIRKS.md. */
    if (unit->ability_flags.support[3] & UNIT_SUPPORT_SET_4_MONSTER_SKILL)
        ai->considered_ability.ct >>= 1;
    else if (unit->ability_flags.support[3] & UNIT_SUPPORT_SET_4_DEFEND)
        ai->considered_ability.ct = 0;
copy_status:
    ai_mem_copy_bytes(ai->considered_ability.parameters.fields.status_infliction,
        g_status_infliction_data[status_id].statuses,
        sizeof(ai->considered_ability.parameters.fields.status_infliction));
}

/* Movement-scenario branch decisions survive a suspended tile-scoring pass. */
static s32 g_target_in_reach = 0; /* range reaches the target, or a reflected action is charging */
static s32 g_target_reach = 0;    /* reach, possibly halved before destination selection */
static s32 g_unit_index = 0;      /* unit/row scan index or Stay Near occupant id */
static s32 g_target_distance = 0;
static s32 g_stay_near_destination_fixed = 0;

static s32 g_protect_allies_status_retry = 0; /* skip status pass after one failed attempt */
static s32 g_protect_allies_status_index = 0; /* status-list cursor preserved for resume */
/* Cancellation-search accounting survives suspension. */
static s32 g_status_candidate_count = 0;
static s32 g_status_insufficient_mp = 0;
static s32 g_status_sufficient_mp = 0;
static s32 g_retreat_policy_stage = 0; /* ai_retreat_stage_e */
static ai_move_retreat_stage_e g_retreat_move_stage = 0;

/*
 * Initialize and dispatch the AI action-selection passes.
 *
 * Resume the selected pass without repeating action-record initialization.
 * Return -1 to suspend and 0 when that pass completes.
 */
static s32 action_select(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 result;

    if (g_ai_workspace_storage.resume_pending) {
        /* Resume: re-enter the step that suspended. */
        switch (g_ai_workspace_storage.initial_selection_stage) {
        case AI_INITIAL_SELECTION_STATUS:
            goto status_action;
        case AI_INITIAL_SELECTION_RETREAT:
            goto retreat_action;
        case AI_INITIAL_SELECTION_AUTOBATTLE:
            goto autobattle_action;
        default:
            goto general_action;
        }
    }
    for (result = 0; result < AI_RANKED_ACTION_COUNT; result++)
        ai_action_rank_reset(&ai->ranked_actions[result]);
    ai_action_rank_reset(&ai->current_action);
    ai_action_rank_reset(&ai->best_action);
    ai_action_rank_reset(&ai->move_baseline_action);
    ai_action_rank_reset(&ai->selected_action);
    foe_proximity_grid_fill();
status_action:
    result = status_imposed_select();
    if (result == -1) {
        ai->initial_selection_stage = AI_INITIAL_SELECTION_STATUS;
        return -1;
    }
    if (result != 0)
        return 0;
    if (((ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
             & (STATUS_BYTE_MASK(STATUS_ID_FROG) | STATUS_BYTE_MASK(STATUS_ID_CRITICAL)))
            && (ai->targetability.live.target_setting_flags & AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE))
        || ((ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_ACT)]
                & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))
            && (ai->acting_unit_decision->flags & AI_DECISION_FOCUS_TARGET))) {
        ai_mem_copy_u16_values(&ai->saved_acting_unit_decision, ai->acting_unit_decision, sizeof(ai_unit_decision_t));
    /* Resume after the save; restore the decision only when this pass ends. */
    retreat_action:
        if (retreat_select() == -1) {
            ai->initial_selection_stage = AI_INITIAL_SELECTION_RETREAT;
            return -1;
        }
        ai_mem_copy_u16_values(ai->acting_unit_decision, &ai->saved_acting_unit_decision, sizeof(ai_unit_decision_t));
        return 0;
    }
autobattle_action:
    result = autobattle_select();
    if (result == -1) {
        ai->initial_selection_stage = AI_INITIAL_SELECTION_AUTOBATTLE;
        return -1;
    }
    if (result != 0)
        return 0;
    ai->resume_pending = 0;
general_action:
    if (general_select() == -1) {
        ai->initial_selection_stage = AI_INITIAL_SELECTION_GENERAL;
        return -1;
    }
    return 0;
}

/*
 * Reset an action's ranking fields and selection defaults.
 *
 * The remaining payload is retained; this is not a whole-record clear.
 */
void ai_action_rank_reset(ai_action_data_t* action) {
    /* Signed view preserves addiu -32767; direct u16 assignment emits ori. */
    action->rank.priority = -0x7fff;
    action->rank.decided = 0;
    action->rank.base_hit_percent = 0;
    action->wait_facing_hint = AI_DIRECTION_CHOOSE_AT_WAIT;
    action->reflected_action = 0;
}

/* Weigh keeping the acting unit's charging action against abandoning it.
 *
 * Returns 0 at once when nothing is charging. The candidate is a move-only
 * action: to the unit's own tile for a Charge ability, else to the scenario's
 * candidate tile. A Song or Dance in progress is just scored and ranked (0).
 * Otherwise the move is simulated with the charge kept, which becomes the
 * selected action, and again with the charge and Charging/Performing cleared
 * from the unit; they are restored afterward, and a suspended pass keeps them
 * cleared until it resumes and ends. Returns 1 when keeping the charge ranks
 * higher, so the selected action stands; 0 otherwise; -1 to suspend.
 */
static s32 keep_charging_select(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* unit = g_ai_workspace_storage.acting_unit;
    action_command_t* action = &unit->action_command;
    ai_charging_stage_e state;

    if (g_ai_workspace_storage.resume_pending) {
        /* Resume: re-enter the step that suspended. */
        state = g_ai_workspace_storage.charging_state;
        if (state == AI_CHARGING_SONG_DANCE)
            goto evaluate_song_dance;
        if (state == AI_CHARGING_WITH_CHARGE)
            goto evaluate_with_charge;
        goto evaluate_without_charge;
    }
    if (unit->charged_ability_ct == 0xff)
        return 0;
    if (g_command_menu_types_by_skillset[unit->action_command.skillset_id] == ACTION_MENU_TYPE_CHARGE) {
        g_ai_workspace_storage.current_action.coords = g_ai_workspace_storage.acting_unit_coords;
    } else {
        g_ai_workspace_storage.current_action.coords = ai->candidate_coords[g_ai_workspace_storage.movement_scenario];
        ai_tile_pickup_update();
    }
    ai->current_action.act_before_move = 0;
    ai->current_action.command.skillset_id = 0;
    if (action->ability_id >= ABILITY_ID_SONG_ANGEL_SONG && action->ability_id <= ABILITY_ID_DANCE_LAST_DANCE) {
    evaluate_song_dance:
        if (ai_action_evaluate() == -1) {
            ai->charging_state = AI_CHARGING_SONG_DANCE;
            return -1;
        }
        ai_best_rank_insert();
        return 0;
    }
evaluate_with_charge:
    if (ai_action_simulate_and_score() == -1) {
        ai->charging_state = AI_CHARGING_WITH_CHARGE;
        return -1;
    }
    ai_mem_copy_u16_values(&ai->selected_action, &ai->current_action, sizeof(ai_action_data_t));
    ai->saved_ability_ct = ai->acting_unit->charged_ability_ct;
    ai->saved_current_status = ai->acting_unit->status_sets.current[0];
    ai->saved_inflicted_status = ai->acting_unit->inflicted_status[0];
    ai->acting_unit->charged_ability_ct = 0xff;
    ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
        &= ~(STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
    ai->acting_unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
        &= ~(STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
evaluate_without_charge:
    if (ai_action_simulate_and_score() != -1) {
        ai->acting_unit->charged_ability_ct = ai->saved_ability_ct;
        ai->acting_unit->status_sets.current[0] = ai->saved_current_status;
        ai->acting_unit->inflicted_status[0] = ai->saved_inflicted_status;
        return rank_compare(&ai->selected_action.rank, &ai->current_action.rank) == AI_RANK_ABOVE;
    }
    ai->charging_state = AI_CHARGING_WITHOUT_CHARGE;
    return -1;
}

/*
 * Run or resume the general autobattle action search.
 *
 * Return -1 to suspend and 0 when the selected action is ready. Keeping a
 * charged action skips the new ability search. The switch preserves the
 * target's completion-return delay slot.
 */
static s32 general_select(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 charging_result;
    ai_general_stage_e resume_state;

    if (g_ai_workspace_storage.resume_pending != 0) {
        /* Resume: re-enter the step that suspended. */
        resume_state = g_ai_workspace_storage.general_selection_stage;
        if (resume_state == AI_GENERAL_MOVEMENT)
            goto evaluate_movement;
        if (resume_state == AI_GENERAL_CHARGING)
            goto evaluate_charging;
        goto evaluate_abilities;
    }
    g_ai_workspace_storage.movement_scenario = 1;
evaluate_movement:
    if (scenarios_build() == -1) {
        ai->general_selection_stage = AI_GENERAL_MOVEMENT;
        return -1;
    }
    ai->movement_scenario = 0;
    rank_baseline_seed();
    ai->movement_scenario = 1;
evaluate_charging:
    charging_result = keep_charging_select();
    switch (charging_result) {
    case -1:
        ai->general_selection_stage = AI_GENERAL_CHARGING;
        return -1;
    case 1:
        return 0;
    }
    ai->movement_scenario = 1;
    if (target_active_units_select(AI_UNIT_FILTER_ANY, 0) != 0) {
    evaluate_abilities:
        if (ability_list_evaluate() == -1) {
            ai->general_selection_stage = AI_GENERAL_ABILITIES;
            return -1;
        }
    }
    if (ranked_select() == 0) {
        ai->movement_scenario = 0;
        fallback_select();
    }
    return 0;
}

/*
 * Choose the range threshold used to partition movement candidates.
 *
 * A focused enemy uses enemy_ability_min_reach; a focused ally uses ally_ability_min_reach.
 * Without a focused target, use Move. The caller consumes the full register.
 */
static s32 target_reach_get(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    ai_unit_decision_t* acting = g_ai_workspace_storage.acting_unit_decision;
    if (acting->flags & AI_DECISION_FOCUS_TARGET) {
        if (ai->unit_decisions[acting->main_target_id].enemy_flag != 0) {
            return acting->enemy_ability_min_reach;
        }
        return acting->ally_ability_min_reach;
    }
    return g_ai_workspace_storage.acting_unit_move;
}

enum {
    AI_BASE_HIT_PERCENT = 100,
    AI_TILE_ROW_BYTE_STRIDE = AI_GRID_COLUMN_COUNT * sizeof(s16),
    AI_TILE_LEVEL_BYTE_STRIDE = AI_GRID_ROW_COUNT * AI_TILE_ROW_BYTE_STRIDE
};

/* 0x5f4; kept as a separate integer term so the target's addu/addiu order
 * survives (a typed &ai->tile_priority[...] folds the offset first). */
#define AI_TILE_PRIORITY_OFFSET offsetof(ai_workspace_t, tile_priority)

/*
 * Seed the move-only baseline action, which a ranked action must beat
 * (ai_best_rank_insert), with 100% hit and the score of simply
 * moving to the scenario's candidate tile.
 *
 * tile_priority holds each tile's move-only score negated, so the destination
 * search can take the lowest value; negating it again restores the score. The
 * grid is 16 columns x 18 rows of 16-bit values per map level, read as the
 * stored unsigned pattern before the negation.
 */
static void rank_baseline_seed(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    ai_coords_t* coords;
    u8* level_row;
    u8 scenario = g_ai_workspace_storage.movement_scenario;
    u32 row;

    g_ai_workspace_storage.move_baseline_action.rank.decided = 0;
    g_ai_workspace_storage.move_baseline_action.rank.base_hit_percent = AI_BASE_HIT_PERCENT;
    coords = &ai->candidate_coords[scenario];
    level_row = (u8*)ai + coords->fields.layer * AI_TILE_LEVEL_BYTE_STRIDE + AI_TILE_PRIORITY_OFFSET;
    row = coords->fields.y * AI_TILE_ROW_BYTE_STRIDE + (u32)level_row;
    g_ai_workspace_storage.move_baseline_action.rank.priority = -*(u16*)(coords->fields.x * 2 + row);
}

/* Prepare the general search's three movement scenarios and their candidate
 * destinations.
 *
 * Scenario 0 holds every reachable tile. When the decision's target is within
 * reach of the unit's tile (target_reach_get), scenario 1 keeps
 * every reachable tile and candidate 1 is the best-scoring one; a free,
 * reachable stay-near tile is instead taken as is. Otherwise scenario 1 keeps
 * the tiles closer to the target and candidate 1 is the best-scoring tile
 * within reach of it, else the nearest; a non-player team halves the reach
 * one time in seven. Scenario 2 holds the reachable tiles outside scenario 1.
 * Candidate 0 is a tile closer to a focused target in reach when the unit has
 * not acted, else candidate 1. While the unit is low on HP or MP, crystals
 * count as distance 1.
 *
 * Return -1 when a child suspends and 0 after all scenarios are prepared.
 * 32-bit scratch values preserve branch decisions across suspension.
 */
static s32 scenarios_build(void) {
    ai_unit_decision_t* decision;
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* unit;
    s32 limit;
    s32 low;
    s32 high;

    g_ai_workspace_storage.movement_scenario = 1;
    decision = g_ai_workspace_storage.acting_unit_decision;
    if (g_ai_workspace_storage.resume_pending) {
        /* Resume: re-enter the step that suspended. */
        if (g_ai_workspace_storage.movement_decision_state == AI_MOVEMENT_TARGET_DISTANCE)
            goto fill_target_distance;
        goto score_tiles;
    }
    if ((decision->flags & AI_DECISION_FOCUS_TARGET)
        && (decision->main_target_id == g_ai_workspace_storage.acting_unit_id
            || ((ai->unit_decisions[decision->main_target_id].targeting_flags_2 >> 4)
                & (AI_TARGET_UNTARGETABLE >> 4)))) {
        ai_mem_zero_32bit_values(ai->tile_target_distance, sizeof(ai->tile_target_distance));
    } else {
    fill_target_distance:
        if (target_distance_grid_fill(&decision->target) == -1) {
            ai->movement_decision_state = AI_MOVEMENT_TARGET_DISTANCE;
            return -1;
        }
        if ((decision->targeting_flags_2 & (AI_TARGET_HP_BELOW_HALF | AI_TARGET_MP_CONSTRAINED))
            && !(ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
                & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE))) {
            for (g_unit_index = 0; g_unit_index < UNIT_SLOT_COUNT; g_unit_index++) {
                unit = &g_unit_stats[g_unit_index];
                if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE
                    && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                        & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))) {
                    ai_unit_coordinates_copy(g_unit_index, &ai->considered_unit_coords);
                    ai->tile_target_distance[ai->considered_unit_coords.fields.layer]
                                            [ai->considered_unit_coords.fields.y][ai->considered_unit_coords.fields.x]
                        = 1;
                }
            }
        }
    }
    g_target_distance = ai->tile_target_distance[ai->acting_unit_coords.fields.layer][ai->acting_unit_coords.fields.y]
                                                [ai->acting_unit_coords.fields.x];
    g_target_reach = target_reach_get();
    limit = g_target_distance;
    g_stay_near_destination_fixed = 0;
    if (g_target_reach >= limit || (decision->targeting_flags_1 & AI_TARGETING_FLAG_1_REFLECTED_ACTION)) {
        g_target_in_reach = 1;
        if ((decision->flags & AI_DECISION_STAY_NEAR_COORDINATES)
            && ((g_unit_index = ai_tile_unit_find(&decision->target)) == ai->acting_unit_id
                || g_unit_index == UNIT_ID_NONE)
            && (ai->reachable_tiles[0][decision->target.fields.layer][decision->target.fields.y]
                & (0x8000 >> decision->target.fields.x))) {
            ai->candidate_coords[ai->movement_scenario] = decision->target;
            candidate_actor_reachability_set();
            g_stay_near_destination_fixed = 1;
        } else {
            target_distance_tiles_filter(ai->acting_unit_move + g_target_distance);
        }
    } else {
        g_target_in_reach = 0;
        target_distance_tiles_filter(limit - 1);
    }
score_tiles:
    if (reachable_tiles_score() == -1) {
        ai->movement_decision_state = AI_MOVEMENT_SCORE_TILES;
        return -1;
    }
    if (!g_stay_near_destination_fixed) {
        if (g_target_in_reach) {
            destination_select(0x7fffffff, AI_DESTINATION_PRIORITY_FIRST);
        } else {
            if (ai->acting_unit_team && rand() % 7 == 0)
                g_target_reach /= 2;
            destination_with_nearest_fallback_select(g_target_reach);
        }
    }
    /* Scenario 2: the reachable tiles outside scenario 1. The XOR is that
     * difference because scenario 1 is filtered from scenario 0. */
    ai->movement_scenario = 2;
    for (g_unit_index = 0; g_unit_index < ai->terrain_depth; g_unit_index++) {
        low = ai->reachable_tiles[0][0][g_unit_index] ^ ai->reachable_tiles[1][0][g_unit_index];
        high = ai->reachable_tiles[0][1][g_unit_index] ^ ai->reachable_tiles[1][1][g_unit_index];
        ai->reachable_tiles[2][0][g_unit_index] = low;
        ai->reachable_tiles[2][1][g_unit_index] = high;
    }
    ai->candidate_coords[2] = ai->candidate_coords[1];
    ai->movement_scenario = 0;
    if (!ai->acting_unit->action_taken && (decision->flags & AI_DECISION_FOCUS_TARGET) && g_target_in_reach
        && !(decision->targeting_flags_1 & AI_TARGETING_FLAG_1_REFLECTED_ACTION)) {
        destination_with_range_floor_select(g_target_distance - 1);
    } else {
        ai->candidate_coords[0] = ai->candidate_coords[1];
    }
    ai->reachable_tiles[1][ai->acting_unit_coords.fields.layer][ai->acting_unit_coords.fields.y]
        |= 0x8000 >> ai->acting_unit_coords.fields.x;
    return 0;
}

/*
 * Select a destination within distance_limit of the main target. A unit with
 * a ranged ability need not come closer than 3 to an enemy target: smaller
 * limits are raised to 3. Selection still uses the ordinary unrestricted
 * distance-first fallback.
 */
static void destination_with_range_floor_select(s32 distance_limit) {
    ai_unit_decision_t* decision = g_ai_workspace_storage.acting_unit_decision;
    if (g_ai_workspace_storage.unit_decisions[decision->main_target_id].enemy_flag != 0
        && (decision->targeting_flags_1 & AI_TARGETING_FLAG_1_HAS_RANGED_ABILITY) && distance_limit < 3) {
        distance_limit = 3;
    }
    destination_with_nearest_fallback_select(distance_limit);
}

/* Prefer priority within range, falling back to the nearest reachable tile.
 *
 * The fallback removes the distance bound. Neither pass requires a positive
 * priority. */
static void destination_with_nearest_fallback_select(s32 range) {
    if (destination_select(range, AI_DESTINATION_PRIORITY_FIRST) == 0) {
        destination_select(0x7fffffff, AI_DESTINATION_DISTANCE_FIRST);
    }
}

/*
 * Select a reachable destination using priority-first or distance-first ordering.
 *
 * Return 1 after selecting a tile, or 0 without changing the destination.
 * Both orders favor the last scanned tile when their two ranking values tie.
 * Integer address stages preserve the target's shared grid-index calculation;
 * direct multidimensional indexing changes the loop code. Strides and field
 * offsets come from the workspace layout, with 16 tiles in each row.
 */
static s32 destination_select(s32 max_distance, destination_order_e order) {
    s32 x, y, level;
    s32 best_score = 0x7fffffff;
    s32 best_distance = 0x7fffffff;
    ai_workspace_t* ai = &g_ai_workspace_storage;
    ai_coords_t* destination = &ai->candidate_coords[g_ai_workspace_storage.movement_scenario];
    u16 row;
    s32 distance, score;

    for (level = 0; level < BATTLE_MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->terrain_depth; y++) {
            {
                u32 scenario_address = ai->movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
                u32 level_address;
                scenario_address += offsetof(ai_workspace_t, reachable_tiles);
                level_address = level * sizeof(ai->reachable_tiles[0][0]) + scenario_address;
                row = *(u16*)(y * sizeof(ai->reachable_tiles[0][0][0]) + level_address);
            }
            if (row != 0) {
                for (x = 0; x < ai->terrain_width; x++) {
                    if ((row << x) & 0x8000) {
                        u32 level_tile_index = y << 4;
                        u32 tile_index;
                        u32 byte_address;
                        level_tile_index += x;
                        tile_index = level * sizeof(ai->tile_target_distance[0]) + level_tile_index;
                        byte_address = tile_index + (u32)ai;
                        distance = *(u8*)(byte_address + offsetof(ai_workspace_t, tile_target_distance));
                        if (max_distance < distance) {
                            continue;
                        }
                        {
                            s32 proximity = *(u8*)(byte_address + offsetof(ai_workspace_t, tile_foe_proximity));
                            score = *(u16*)(tile_index * sizeof(ai->tile_priority[0][0][0]) + (u32)ai
                                + offsetof(ai_workspace_t, tile_priority));
                            /* Compare the packed priority and proximity as a signed score. */
                            score = score << 16;
                            score += proximity;
                        }
                        if (order != AI_DESTINATION_PRIORITY_FIRST) {
                            /* max_distance now tracks the best distance: a later tile must be at
                             * least as close, and an equally close one must score no worse. */
                            if (max_distance == distance && best_score < score) {
                                continue;
                            }
                            max_distance = distance;
                        } else {
                            if (best_score < score) {
                                continue;
                            }
                            if (score == best_score && best_distance < distance) {
                                continue;
                            }
                        }
                        best_score = score;
                        best_distance = distance;
                        destination->fields.x = x;
                        destination->fields.y = y;
                        destination->fields.layer = level;
                        destination->fields.zero = 0;
                    }
                }
            }
        }
    }
    return best_score != 0x7fffffff;
}

/*
 * Build a reachable-tile mask within the target-distance limit.
 *
 * Filter scenario 0 into the selected scenario, accepting distances equal
 * to the limit. Return 1 if any tile qualifies; otherwise copy all scenario
 * 0 rows and return 0. The destination is cleared before reading the source,
 * so callers select scenario 1 to preserve the scenario 0 mask.
 */
static s32 target_distance_tiles_filter(s32 distance_limit) {
    s32 found = 0;
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 x, y, level;
    u16 row;

    for (x = 0; x < AI_GRID_ROW_COUNT; x++) {
        ai->reachable_tiles[ai->movement_scenario][0][x] = 0;
        ai->reachable_tiles[ai->movement_scenario][1][x] = 0;
    }
    for (level = 0; level < BATTLE_MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->terrain_depth; y++) {
            row = ai->reachable_tiles[0][level][y];
            if (row != 0) {
                for (x = 0; x < ai->terrain_width; x++) {
                    if (((row << x) & 0x8000) && distance_limit >= ai->tile_target_distance[level][y][x]) {
                        ai->reachable_tiles[ai->movement_scenario][level][y] |= 0x8000 >> x;
                        found = 1;
                    }
                }
            }
        }
    }
    if (found == 0) {
        ai_mem_copy_u16_values(
            ai->reachable_tiles[ai->movement_scenario], ai->reachable_tiles[0], sizeof(ai->reachable_tiles[0]));
        return 0;
    }
    return 1;
}

/* 1 when the unit is untargetable or dead without Reraise per its AI decision
 * record, or petrified (Petrify is the top bit of status byte 1). */
static s32 target_is_out_of_play(s32 unit_id) {
    u8 flags = g_ai_workspace_storage.unit_decisions[unit_id].targeting_flags_2;

    if (((flags >> 4) & (AI_TARGET_UNTARGETABLE >> 4)) || ((flags >> 5) & (AI_TARGET_DEAD_WITHOUT_RERAISE >> 5))) {
        return 1;
    }
    return g_unit_stats[unit_id].status_sets.current[1] >> 7;
}

/* Excludes absent, inert, mounted, or hidden enemies from ordinary AI targeting.
 *
 * The native code deliberately uses only 4 bits for the rider's partner ID,
 * unlike other mount consumers' 5-bit mask. A Transparent enemy is
 * untargetable unless it is a rider whose mount is also Transparent. */
s32 ai_target_is_untargetable(s32 unit_idx) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* unit;
    s32 mount;

    unit = &g_unit_stats[unit_idx];
    if (unit->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return 1;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return 1;
    }
    mount = unit->mount_info;
    if (mount & UNIT_MOUNT_INFO_FLAG_MOUNT) {
        return 1;
    }
    if (ai->unit_decisions[unit_idx].enemy_flag == 0) {
        return 0;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)] & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))
        && (!(mount & UNIT_MOUNT_INFO_FLAG_RIDER)
            || !(g_unit_stats[mount & 0xf].status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT)))) {
        return 1;
    }
    return 0;
}

/*
 * Compare the ranking fields of two actions.
 *
 * Compare the decided marker, signed priority, then hit percentage. Every
 * candidate holds 0 in the marker, so priority is the first key that differs.
 */
static rank_order_e rank_compare(const ai_action_rank_t* first, const ai_action_rank_t* second) {
    u8 decided_a, decided_b;
    s16 priority_a, priority_b;
    u8 hit_a, hit_b;

    decided_a = first->decided;
    decided_b = second->decided;
    if (decided_a > decided_b)
        return AI_RANK_ABOVE;
    if (decided_a < decided_b)
        return AI_RANK_BELOW;
    priority_a = first->priority;
    priority_b = second->priority;
    if (priority_a > priority_b)
        return AI_RANK_ABOVE;
    if (priority_a < priority_b)
        return AI_RANK_BELOW;
    hit_a = first->base_hit_percent;
    hit_b = second->base_hit_percent;
    if (hit_a > hit_b)
        return AI_RANK_ABOVE;
    if (hit_a < hit_b)
        return AI_RANK_BELOW;
    return AI_RANK_EQUAL;
}

/*
 * Return whether the first action outranks the second.
 *
 * Rank ties prefer lower foe proximity; equal proximity is not an improvement.
 * Reusing the working scalar for the level preserves target register lifetimes.
 */
s32 ai_rank_is_higher(const ai_action_data_t* first, const ai_action_data_t* second) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    rank_order_e comparison;
    s32 proximity_a;
    s32 proximity_b;
    s32 value;

    comparison = rank_compare(&first->rank, &second->rank);
    value = 1;
    if (comparison == AI_RANK_ABOVE)
        return value;
    if (comparison == AI_RANK_EQUAL) {
        value = first->coords.fields.layer;
        proximity_a = ai->tile_foe_proximity[value][first->coords.fields.y][first->coords.fields.x];
        proximity_b
            = ai->tile_foe_proximity[second->coords.fields.layer][second->coords.fields.y][second->coords.fields.x];
        return proximity_a < proximity_b;
    }
    return 0;
}

/* Score the considered ability cast from the unit's own tile (act, then move)
 * and, when the action's destination tile differs and allows the ability, cast
 * from there (move, then act). -1 suspends. */
s32 ai_move_origins_evaluate(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;

    if (g_ai_workspace_storage.resume_pending != 0) {
        /* Resume: re-enter the step that suspended. */
        if (g_ai_workspace_storage.cast_origin_stage != AI_CAST_ORIGIN_ACT_BEFORE_MOVE) {
            goto score_move_before_act;
        }
        goto score_act_before_move;
    }
    if (ai_origin_is_allowed(&ai->acting_unit_coords) != 0) {
        g_ai_workspace_storage.current_action.act_before_move = 1;
    score_act_before_move:
        if (ai_action_evaluate() == -1) {
            ai->cast_origin_stage = AI_CAST_ORIGIN_ACT_BEFORE_MOVE;
            return -1;
        }
    }
    if (ai->current_action.coords.word == ai->acting_unit_coords.word) {
        return 0;
    }
    if (ai_origin_is_allowed(&ai->current_action.coords) == 0) {
        return 0;
    }
    ai->current_action.act_before_move = 0;
score_move_before_act:
    if (ai_action_evaluate() != -1) {
        return 0;
    }
    ai->cast_origin_stage = AI_CAST_ORIGIN_MOVE_BEFORE_ACT;
    return -1;
}

/*
 * Insert the best action into the ranked list, then reset it.
 *
 * The candidate must beat the movement baseline and the last ranked entry.
 * Insert through the shift loop's final index to preserve register lifetimes.
 */
void ai_best_rank_insert(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 i;
    s32 j;

    if (rank_compare(&ai->best_action.rank, &ai->move_baseline_action.rank) == AI_RANK_ABOVE) {
        if (ai_rank_is_higher(&ai->best_action, &ai->ranked_actions[AI_RANKED_ACTION_COUNT - 1])) {
            for (i = 0; i < AI_RANKED_ACTION_COUNT; i++) {
                if (ai_rank_is_higher(&ai->best_action, &ai->ranked_actions[i]))
                    break;
            }
            if (i != AI_RANKED_ACTION_COUNT) {
                for (j = AI_RANKED_ACTION_COUNT - 1; i < j; j--)
                    ai_mem_copy_u16_values(
                        &ai->ranked_actions[j], &ai->ranked_actions[j - 1], sizeof(ai_action_data_t));
                ai_mem_copy_u16_values(&ai->ranked_actions[j], &ai->best_action, sizeof(ai_action_data_t));
            }
        }
    }
    ai_action_rank_reset(&ai->best_action);
}

/*
 * Select an action by testing ranked entries in order.
 *
 * Each visited entry consumes a random draw, including zero-hit entries.
 * If every draw fails but any hit rate is nonzero, select the first entry.
 */
static s32 ranked_select(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 i;
    s32 any;
    s32 chance;

    any = 0;
    for (i = 0; i < AI_RANKED_ACTION_COUNT; i++) {
        chance = ai->ranked_actions[i].rank.base_hit_percent;
        if (chance != 0) {
            any = 1;
        }
        if ((rand() % 100) < chance) {
            ai_mem_copy_u16_values(&ai->selected_action, &ai->ranked_actions[i], sizeof(ai_action_data_t));
            return 1;
        }
    }
    if (any == 0) {
        return 0;
    }
    ai_mem_copy_u16_values(&ai->selected_action, &ai->ranked_actions[0], sizeof(ai_action_data_t));
    return 1;
}

/*
 * Choose or resume an action imposed by an AI-control status.
 *
 * Chicken takes precedence over Blood Suck, Confusion and Berserk. Return
 * -1 to suspend, 1 when selection completes, or 0 if none of these applies.
 *
 * Chicken only moves: to the best-scoring reachable tile, with fewer nearby
 * foes breaking ties. Blood Suck attacks the nearest unit without Blood Suck
 * with the Blood Suck entry; Berserk attacks the nearest enemy with Attack, or
 * with the Frog Attack entry for a monster or a frog. Confusion tries the
 * usable abilities in random order on any unit. When no ability is chosen, the
 * unit walks toward its target (the nearest unit under Confusion).
 */
static s32 status_imposed_select(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* unit;
    unit_stats_t* berserk_unit;
    ai_ability_entry_t* entry;
    s32 i, target_or_count;
    ai_coords_t coords;
    u8 status_flags;

    if (g_ai_workspace_storage.resume_pending) {
        if (g_ai_workspace_storage.action_selection_phase == AI_STATUS_SELECTION_CHICKEN)
            goto chicken;
        goto movement;
    }
    unit = g_ai_workspace_storage.acting_unit;
    g_ai_workspace_storage.movement_scenario = 0;
    status_flags = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)];
    if (status_flags & STATUS_BYTE_MASK(STATUS_ID_CHICKEN)) {
    chicken:
        if (reachable_tiles_score() == -1) {
            ai->action_selection_phase = AI_STATUS_SELECTION_CHICKEN;
            return -1;
        }
        ai_mem_zero_32bit_values(ai->tile_target_distance, sizeof(ai->tile_target_distance));
        destination_select(255, AI_DESTINATION_DISTANCE_FIRST);
        coords = ai->candidate_coords[0];
        ai->selected_action.act_before_move = 0;
        goto save_movement;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_BLOOD_SUCK)] & STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)) {
        target_or_count = ai_target_find_nearest(AI_NEAREST_WITHOUT_BLOOD_SUCK);
        entry = &ai->hardcoded_status_abilities[0];
        ai->hardcoded_status_abilities[0].packed_id |= g_ai_workspace_storage.acting_unit_id
            << AI_ABILITY_ENTRY_UNIT_SHIFT;
    load_targeted_ability:
        ai_ability_entry_load(entry);
        {
            /* Direct member indexing changes this reverse clear. Keep the
             * target's base cursor and derive the field offset from its member. */
            u32 cursor;
            i = UNIT_SLOT_COUNT - 1;
            cursor = (u32)ai + i;
            do {
                *(u8*)(cursor + offsetof(ai_workspace_t, targetability.live.unit_targetable)) = 0;
                i--;
                cursor--;
            } while (i >= 0);
        }
        ai->targetability.live.unit_targetable[target_or_count] = 1;
        if (random_select())
            return 1;
    copy_target:
        ai_unit_coordinates_copy(target_or_count, &ai->considered_unit_coords);
    movement:
        if (target_distance_grid_fill(&ai->considered_unit_coords) == -1) {
            ai->action_selection_phase = AI_STATUS_SELECTION_TARGET_DISTANCE;
            return -1;
        }
        destination_select(0x7fffffff, AI_DESTINATION_DISTANCE_FIRST);
        coords = ai->candidate_coords[ai->movement_scenario];
        ai->selected_action.act_before_move = 0;
    save_movement:
        /* Movement-only selection leaves the other action fields unchanged. */
        ai->selected_action.command.skillset_id = 0;
        ai->selected_action.coords = coords;
        goto success; /* a direct return 1 here adds an instruction */
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)] & STATUS_BYTE_MASK(STATUS_ID_CONFUSION)) {
        if (target_active_units_select(AI_UNIT_FILTER_ANY, 255)) {
            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                ai->targetability.live.unit_targetable[i] = 0;
                if (!target_is_out_of_play(i))
                    ai->targetability.live.unit_targetable[i] = ai->targetability.live.unit_active[i];
            }
            /* The aligned skillset/usage 16-bit value preserves target LHU/SH;
             * byte-only usage accesses change the instructions. */
            target_or_count = 0;
            for (i = 0; i < AI_ABILITY_LIST_COUNT; i++) {
                entry = &ai->ability_lists[ai->acting_unit_compact_index][i];
                if (entry->skillset == SKILLSET_ID_NONE)
                    break;
                if (entry->usage.policy.usable)
                    target_or_count++;
            }
            if (target_or_count) {
                do {
                    i = rand() % target_or_count;
                    {
                        s32 n;
                        for (n = 0; n < AI_ABILITY_LIST_COUNT; n++) {
                            entry = &ai->ability_lists[ai->acting_unit_compact_index][n];
                            if (entry->usage.policy.usable) {
                                if (i == 0)
                                    break;
                                i--;
                            }
                        }
                    }
                    ai_ability_entry_load(entry);
                    if (random_select())
                        return 1;
                    target_or_count--;
                    entry->usage.policy.usable = 0;
                } while (target_or_count);
            }
        }
        target_or_count = ai_target_find_nearest(AI_NEAREST_UNIT);
        goto copy_target;
    }
    if (status_flags & STATUS_BYTE_MASK(STATUS_ID_BERSERK)) {
        target_or_count = ai_target_find_nearest(AI_NEAREST_ENEMY);
        berserk_unit = g_ai_workspace_storage.acting_unit;
        if (berserk_unit->primary_skillset < SKILLSET_ID_MONSTER_FIRST
            && !(berserk_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
                & STATUS_BYTE_MASK(STATUS_ID_FROG))) {
            entry = &ai->ability_lists[g_ai_workspace_storage.acting_unit_compact_index][0];
        } else {
            entry = &ai->hardcoded_status_abilities[1];
            ai->hardcoded_status_abilities[1].packed_id |= ai->acting_unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
        }
        goto load_targeted_ability;
    }
    return 0;
success:
    return 1;
}

/*
 * Choose a random action from the candidate movement and target masks.
 *
 * Math Skill first chooses learned ability and parameter combinations. Return
 * 1 after copying the current action, including fields unchanged by this pass,
 * or 0 when selection fails.
 *
 * A map-tile ability takes random reachable tiles until one allows it. Any
 * other ability takes random origins until a random tile in range from one
 * works; a follow-target ability aims at a unit or a tile at random (tiles
 * when no unit can be reached), and a unit aim needs a unit on the tile. Used
 * for status-imposed actions only; nothing is scored.
 */
static s32 random_select(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 value, skillset;
    u16* abilities;
    s32 index_or_count;

    g_ai_workspace_storage.current_action.act_before_move = 0;
    g_ai_workspace_storage.current_action.command.skillset_id = ai->considered_ability.skillset;
    g_ai_workspace_storage.current_action.command.ability_id = ai->considered_ability.ability_id;
    g_ai_workspace_storage.current_action.command.item_id = ai->considered_ability.parameters.fields.item_id;
    if (g_command_menu_types_by_skillset[ai->considered_ability.skillset] == ACTION_MENU_TYPE_MATH_SKILL) {
        do {
            skillset = g_ai_math_source_skillsets[rand() % AI_MATH_SOURCE_SKILLSET_COUNT];
            abilities = ability_skillset_list_build(skillset, SKILLSET_ABILITY_FILTER_ACTION);
            index_or_count = rand() % HUMAN_SKILLSET_ACTION_SLOT_COUNT;
            value = abilities[index_or_count];
        } while (value == 0 || !(g_ability_data[value].ai_flags.flags_4 & AI_ABILITY_FLAG_4_USABLE_BY_AI)
            || !(g_ability_range_data[value].flags_3 & ABILITY_SECONDARY_FLAG_3_CALCULATOR_ELIGIBLE)
            || !ai_ability_is_known(ai->acting_unit_id, skillset, index_or_count));
        ai->current_action.command.ability_id = value;
        do {
            value = rand() % CALCULATOR_ATTRIBUTE_ABILITY_COUNT;
        } while (!ai_ability_is_known(ai->acting_unit_id, ai->considered_ability.skillset, value));
        ai->current_action.command.calculator_type = value + ABILITY_ID_MATH_FIRST;
        do {
            value = rand() % CALCULATOR_MULTIPLE_ABILITY_COUNT + CALCULATOR_ATTRIBUTE_ABILITY_COUNT;
        } while (!ai_ability_is_known(ai->acting_unit_id, ai->considered_ability.skillset, value));
        ai->current_action.command.calculator_multiplier = value + ABILITY_ID_MATH_FIRST;
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_TARGET_MAP_TILES) {
        ai_mem_copy_u16_values(ai->targetable_tiles, ai->reachable_tiles[0], sizeof(ai->targetable_tiles));
        ai->cast_origin_tile_count = ai_flagged_tiles_count(ai->targetable_tiles);
        if (!ai->cast_origin_tile_count)
            return 0;
        do {
            if (!random_tile_take(ai->targetable_tiles, &ai->current_action.coords, ai->cast_origin_tile_count))
                return 0;
            ai->cast_origin_tile_count--;
            if (ai_origin_is_allowed(&ai->current_action.coords)) {
                ai->current_action.wait_facing_hint = AI_DIRECTION_OVERLAP;
                goto selected;
            }
        } while (ai->cast_origin_tile_count);
    } else {
        /* Removing this single-attempt loop changes register allocation. */
        do {
            if (ai->considered_ability.ai_flags.word & AI_ABILITY_FOLLOW_TARGET)
                ai->current_action.command.targeting_type = rand() % 2 + TARGET_TYPE_TILE;
            else
                ai->current_action.command.targeting_type = TARGET_TYPE_TILE;
            ai_cast_aim_tiles_build();
            if (!ai->cast_origin_tile_count) {
                if (ai->current_action.command.targeting_type == TARGET_TYPE_UNIT) {
                    ai->current_action.command.targeting_type = TARGET_TYPE_TILE;
                    ai_cast_aim_tiles_build();
                }
                if (!ai->cast_origin_tile_count)
                    break;
            }
            do {
                if (!random_tile_take(ai->cast_origin_tiles, &ai->current_action.coords, ai->cast_origin_tile_count))
                    return 0;
                ai->cast_origin_tile_count--;
                /* Keep the original count even when a rejected target bit is removed. */
                index_or_count = origin_ability_range_calculate();
                if (index_or_count) {
                    while (random_tile_take(ai->targetable_tiles, &ai->considered_unit_coords, index_or_count)) {
                        ai->current_action.command.target_position.vx = ai->considered_unit_coords.fields.x;
                        ai->current_action.command.target_position.vz = ai->considered_unit_coords.fields.y;
                        ai->current_action.command.target_position.vy = ai->considered_unit_coords.fields.layer;
                        ai->current_action.wait_facing_hint
                            = ai_target_facing_find(&ai->considered_unit_coords, &ai->current_action.coords);
                        if (ai->current_action.command.targeting_type == TARGET_TYPE_UNIT
                            && (ai->current_action.command.target_id = ai_tile_unit_find(&ai->considered_unit_coords))
                                == UNIT_ID_NONE)
                            continue;
                        goto selected;
                    }
                }
            } while (ai->cast_origin_tile_count);
        } while (0);
    }
    return 0;
selected: /* shared success tail; a copy at each site adds an instruction */
    ai_mem_copy_u16_values(&ai->selected_action, &ai->current_action, sizeof(ai_action_data_t));
    return 1;
}

/* Side of origin the target tile lies on, split along the two diagonals (south
 * is lower y, west lower x). A tile on a diagonal counts as south (to the
 * south-west), west (north-west), north (north-east) or east (south-east).
 * OVERLAP for the same tile. */
ai_direction_e ai_target_facing_find(const ai_coords_t* target, const ai_coords_t* origin) {
    s8 target_y = target->fields.y;
    s8 target_x = target->fields.x;
    s8 origin_x = origin->fields.x;
    s8 origin_y = origin->fields.y;
    s32 diagonal_y_minus_x = ((target_y - target_x) + origin_x) - origin_y;
    s32 diagonal_y_plus_x = ((target_y + target_x) - origin_x) - origin_y;

    if (diagonal_y_minus_x <= 0) {
        if (diagonal_y_plus_x < 0)
            return AI_DIRECTION_SOUTH;
    } else {
        if (diagonal_y_plus_x <= 0)
            return AI_DIRECTION_WEST;
    }
    if (diagonal_y_minus_x >= 0 && diagonal_y_plus_x > 0)
        return AI_DIRECTION_NORTH;
    if (diagonal_y_minus_x < 0 && diagonal_y_plus_x >= 0)
        return AI_DIRECTION_EAST;
    return AI_DIRECTION_OVERLAP;
}

/* Intersect the aim tiles with the current ability's range.
 *
 * The acting unit's coordinates are restored before every return. */
static s32 origin_ability_range_calculate(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    battle_map_tile_t* tile;
    s32 count;
    s32 level;
    s32 y;
    s32 x;
    u16* target_rows;
    u16* origin_rows;
    u16 row;
    s32 tile_mask;
    u32 level_address;
    char unused[24]; /* the target's frame reserves 24 bytes nothing uses */

    ai_acting_unit_place(&ai->current_action.coords);
    count = 0;
    if (ai->considered_ability.parameters.fields.range != 0) {
        if (target_range_set_for_action(&ai->current_action.command) == TARGET_PANELS_NONE) {
            ai_acting_unit_place(&ai->acting_unit_coords);
            return 0;
        }
    } else {
        x = 0;
        tile = g_battle_map_tile_data;
        do {
            x++;
            tile->ceiling_depth_and_marks.ability_range = 0;
            tile++;
        } while (x < BATTLE_MAP_TILE_COUNT);
        g_battle_map_tile_data[ai->current_action.coords.fields.y * ai->terrain_width
            + ai->current_action.coords.fields.x
            + (ai->current_action.coords.fields.layer * BATTLE_MAP_TILES_PER_LAYER)]
            .ceiling_depth_and_marks.ability_range = 1;
    }

    level = 0;
    /* Direct typed row pointers change the original loop bytes. */
    for (level_address = (u32)ai; level < BATTLE_MAP_TERRAIN_LAYER_COUNT;
        level++, level_address += AI_GRID_ROW_COUNT * sizeof(u16)) {
        for (y = 0; y < ai->terrain_depth; y++) {
            target_rows = (u16*)(level_address + offsetof(ai_workspace_t, targetable_tiles));
            origin_rows = (u16*)(level_address + offsetof(ai_workspace_t, aim_tiles));
            target_rows[y] = 0;
            row = origin_rows[y];
            if (row != 0) {
                for (x = 0; x < ai->terrain_width; x++) {
                    tile_mask = row << x;
                    if (tile_mask & 0x8000) {
                        if (g_battle_map_tile_data[y * ai->terrain_width + x + (level * BATTLE_MAP_TILES_PER_LAYER)]
                                .ceiling_depth_and_marks.ability_range) {
                            count++;
                            tile_mask = 0x8000;
                            target_rows[y] |= tile_mask >> x;
                        }
                    }
                }
            }
        }
    }
    ai_acting_unit_place(&ai->acting_unit_coords);
    return count;
}

/*
 * Remove a randomly selected tile from a candidate mask.
 *
 * The supplied count determines the random ordinal. Return 0 when it is 0
 * or no corresponding set bit is found; otherwise write coordinates and return 1.
 */
static s32 random_tile_take(
    u16 tiles[BATTLE_MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT], ai_coords_t* coordinates, s32 count) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 selected;
    s32 level, y, x;
    u16 row;

    if (count == 0) {
        return 0;
    }
    selected = rand() % count;
    for (level = 0; level < BATTLE_MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->terrain_depth; y++) {
            row = tiles[level][y];
            if (row != 0) {
                for (x = 0; x < ai->terrain_width; x++) {
                    if ((row << x) & 0x8000) {
                        if (selected == 0) {
                            coordinates->fields.x = x;
                            coordinates->fields.y = y;
                            coordinates->fields.layer = level;
                            coordinates->fields.zero = 0;
                            tiles[level][y] ^= 0x8000 >> x;
                            return 1;
                        }
                        selected--;
                    }
                }
            }
        }
    }
    return 0;
}

/*
 * Count candidate tiles within the map bounds on both levels.
 *
 * Each level has 18 rows of 16-bit values, with bit 15 representing x = 0.
 */
s32 ai_flagged_tiles_count(u16 rows[BATTLE_MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT]) {
    ai_workspace_t* ai;
    s32 count = 0;
    s32 level;
    s32 y;
    s32 x;
    u16 row;

    ai = &g_ai_workspace_storage;
    for (level = 0; level < BATTLE_MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->terrain_depth; y++) {
            row = rows[level][y];
            if (row != 0) {
                for (x = 0; x < ai->terrain_width; x++) {
                    if (((row << x) & 0x8000) != 0) {
                        count++;
                    }
                }
            }
        }
    }
    return count;
}

/*
 * Select active units for the AI ability search.
 *
 * Rebuild all per-unit flags and return whether any unit was selected.
 * target_id == 0xff excludes the acting unit. Confusion bypasses the
 * considered flag, but does not bypass untargetability. AI_UNIT_FILTER_ANY and any
 * value outside the enum allow either side.
 */
static s32 target_active_units_select(unit_filter_e mode, s32 target_id) {
    s32 any_active = 0;
    s32 i;
    ai_workspace_t* ai = &g_ai_workspace_storage;
    ai_unit_decision_t* decision;
    unit_stats_t* unit;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        decision = &ai->unit_decisions[i];
        ai->targetability.live.unit_active[i] = 0;
        if (decision->targeting_flags_2 & AI_TARGET_UNTARGETABLE)
            continue;
        if (!(g_unit_stats[i].status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
                & STATUS_BYTE_MASK(STATUS_ID_CONFUSION))
            && !(decision->flags & AI_DECISION_CONSIDERED))
            continue;
        if (i == ai->acting_unit_id && target_id == 0xff)
            continue;
        /* Target dispatch uses a signed comparison, including values outside the enum. */
        switch ((s32)mode) {
        case AI_UNIT_FILTER_SPECIFIC:
            if (i != target_id)
                continue;
            break;
        case AI_UNIT_FILTER_ENEMIES:
            if (!decision->enemy_flag)
                continue;
            break;
        case AI_UNIT_FILTER_ALLIES:
            if (decision->enemy_flag)
                continue;
            break;
        case AI_UNIT_FILTER_STATUS:
            unit = &g_unit_stats[i];
            /* Target lw at 0x80197e10 reads the aligned first four status bytes. */
            if (!(*(u32*)&unit->status_sets.current[0]
                    & (STATUS_PACKED_MASK(STATUS_ID_DEAD) | STATUS_PACKED_MASK(STATUS_ID_PETRIFY)
                        | STATUS_PACKED_MASK(STATUS_ID_CRITICAL)))
                && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
                    & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE)))
                continue;
            break;
        }
        ai->targetability.live.unit_active[i] = 1;
        any_active = 1;
    }
    return any_active;
}

/* Evaluate each entry of the acting unit's ability list in turn: 34 entries at
 * most, ended early by SKILLSET_ID_NONE. -1 suspends, and the next call
 * resumes inside the loop. */
static s32 ability_list_evaluate(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    ai_ability_entry_t* entry;

    if (g_ai_workspace_storage.resume_pending != 0) {
        goto retry; /* resume the suspended evaluation inside the loop */
    }
    g_ai_workspace_storage.ability_counter = 0;
    do {
        entry = &ai->ability_lists[ai->acting_unit_compact_index][ai->ability_counter];
        if (entry->skillset == SKILLSET_ID_NONE) {
            return 0;
        }
        ai_ability_entry_load(entry);
    retry:
        if (ai_ability_outcome_evaluate() == -1) {
            return -1;
        }
        ai->ability_counter += 1;
    } while (ai->ability_counter < AI_ABILITY_LIST_COUNT);
    return 0;
}

/*
 * Dispatch the acting unit's special automatic-battle behavior.
 *
 * A result of -2 chains recovery into retreat, or fading-life behavior into
 * recovery; recovery never returns -2, so its chain is unreachable (QUIRKS.md).
 * All dispatched routines otherwise collapse success to 1 while preserving the
 * suspended result -1. Any other setting returns 0, leaving the caller to run
 * the general search.
 */
static s32 autobattle_select(void) {
    s32 result;

    /* A -2 result falls back to another policy's case; those jumps stay gotos. */
    switch (g_ai_workspace_storage.autobattle_setting) {
    case AI_AUTOBATTLE_FIGHT_FOR_LIFE:
        if (fight_for_life_select() != -1) {
            return 1;
        }
        break;
    case AI_AUTOBATTLE_PROTECT_ALLIES:
    recovery_policy:
        result = protect_allies_select();
        if (result == -1) {
            return -1;
        }
        if (result == -2) {
            goto retreat_policy;
        }
        return 1;
    case AI_AUTOBATTLE_FADING_LIFE:
        result = save_fading_life_select();
        if (result == -1) {
            return -1;
        }
        if (result == -2) {
            goto recovery_policy;
        }
        return 1;
    case AI_AUTOBATTLE_RETREAT:
    retreat_policy:
        if (retreat_select() != -1) {
            return 1;
        }
        break;
    default:
        return 0;
    }
    return -1;
}

/*
 * Select an action for the TARGETED auto-battle mode, aimed at the unit's
 * auto-battle target.
 *
 * A target that is out of play or no longer an enemy is replaced, and special
 * behavior dropped: a mount by its rider (unless both are Transparent), any
 * other unit by the nearest eligible enemy, or by the first Transparent enemy
 * when none is left. The search then moves toward the target and ranks
 * abilities on it, then on any unit, before the fallback. The target is
 * reloaded from the unit on every resume. Return -1 to suspend and 0 when
 * selection completes.
 */
static s32 fight_for_life_select(void) {
    s32 target = g_ai_workspace_storage.acting_unit->auto_battle_target;
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* unit;
    s32 i, result;

    /* Reload the persistent target before every resume dispatch. */
    if (g_ai_workspace_storage.resume_pending) {
        switch (g_ai_workspace_storage.action_selection_phase) {
        case AI_FIGHT_FOR_LIFE_SELECTION_SCORE_TILES:
            goto score_tiles;
        case AI_FIGHT_FOR_LIFE_SELECTION_MOVEMENT:
            goto movement;
        case AI_FIGHT_FOR_LIFE_SELECTION_CHARGING:
            goto charging;
        case AI_FIGHT_FOR_LIFE_SELECTION_TARGET_ABILITIES:
            goto target_abilities;
        default:
            goto other_abilities;
        }
    }
    if (target_is_out_of_play(target) || !ai->unit_decisions[target].enemy_flag) {
        g_ai_workspace_storage.acting_unit_decision->flags &= ~AI_DECISION_SPECIAL_BEHAVIOR;
        unit = &g_unit_stats[target];
        if (unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) {
            if (!((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                      & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))
                    && (g_unit_stats[unit->mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK]
                            .status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                        & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))))
                target = unit->mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK;
        } else {
            /* A direct member pointer folds the target's base cursor and field
             * displacement together. Derive the member offset to retain its stores. */
            u32 cursor;
            s32 included = 1;
            i = UNIT_SLOT_COUNT - 1;
            cursor = (u32)ai + i;
            do {
                *(u8*)(cursor + offsetof(ai_workspace_t, targetability.live.unit_targetable)) = included;
                i--;
                cursor--;
            } while (i >= 0);
            target = ai_target_find_nearest(AI_NEAREST_ELIGIBLE_ENEMY);
            if (target == ai->acting_unit_id) {
                for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                    unit = &g_unit_stats[i];
                    if (unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE && ai->unit_decisions[i].enemy_flag
                        && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                            & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))) {
                        target = i;
                        break;
                    }
                }
            }
        }
    }
    ai->acting_unit->auto_battle_target = target;
    decision_target_focus(target);
    ai->movement_scenario = 0;
score_tiles:
    if (reachable_tiles_score() == -1) {
        ai->action_selection_phase = AI_FIGHT_FOR_LIFE_SELECTION_SCORE_TILES;
        return -1;
    }
movement:
    if (targeted_build() == -1) {
        ai->action_selection_phase = AI_FIGHT_FOR_LIFE_SELECTION_MOVEMENT;
        return -1;
    }
    ai->movement_scenario = 2;
    rank_baseline_seed();
    ai->movement_scenario = 0;
charging:
    result = keep_charging_select();
    switch (result) {
    case -1:
        ai->action_selection_phase = AI_FIGHT_FOR_LIFE_SELECTION_CHARGING;
        return -1;
    case 1:
        return 0;
    }
    ai->movement_scenario = 0;
    if (target_active_units_select(AI_UNIT_FILTER_SPECIFIC, target)) {
    target_abilities:
        if (ability_list_evaluate() == -1) {
            ai->action_selection_phase = AI_FIGHT_FOR_LIFE_SELECTION_TARGET_ABILITIES;
            return -1;
        }
        if (ranked_select())
            return 0;
    }
    ai->movement_scenario = 1;
    if (target_active_units_select(AI_UNIT_FILTER_ANY, 0)) {
    other_abilities:
        if (ability_list_evaluate() == -1) {
            ai->action_selection_phase = AI_FIGHT_FOR_LIFE_SELECTION_OTHER_ABILITIES;
            return -1;
        }
        if (ranked_select())
            return 0;
    }
    ai->movement_scenario = 2;
    fallback_select();
    return 0;
}

/*
 * Prepare movement scenarios around the policy's designated target.
 *
 * Return -1 while target-distance propagation is suspended, otherwise 0.
 * Protect allies mode limits its initial destination search to 2 when farther away;
 * scenario 2 may approach one tile closer if the actor has not acted.
 */
static s32 targeted_build(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 range;
    u8(*distance_rows)[AI_GRID_COLUMN_COUNT];
    u32 distance_level_base;
    s32 limit;
    s32 distance;
    s32 within_two_tiles;

    if (!g_ai_workspace_storage.resume_pending
        && (g_ai_workspace_storage.acting_unit->auto_battle_target == g_ai_workspace_storage.acting_unit_id
            || ((ai->unit_decisions[g_ai_workspace_storage.acting_unit->auto_battle_target].targeting_flags_2 >> 4)
                & (AI_TARGET_UNTARGETABLE >> 4)))) {
        ai_mem_zero_32bit_values(ai->tile_target_distance, sizeof(ai->tile_target_distance));
    } else if (target_distance_grid_fill(&ai->acting_unit_decision->target) == -1) {
        return -1;
    }
    range = target_reach_get();
    /* Direct typed level indexing reverses the two address-building instructions
     * at +0xbc/+0xc0. Stage only the level base; retain the real member offset
     * and typed row/column indexing for the byte-exact match. */
    distance_level_base = ai->acting_unit_coords.fields.layer * sizeof(ai->tile_target_distance[0]) + (u32)ai;
    distance_rows = (u8(*)[AI_GRID_COLUMN_COUNT])(distance_level_base + offsetof(ai_workspace_t, tile_target_distance));
    limit = range;
    distance = distance_rows[ai->acting_unit_coords.fields.y][ai->acting_unit_coords.fields.x];
    within_two_tiles = distance < 3;
    ai->movement_scenario = 0;
    if (ai->autobattle_setting == AI_AUTOBATTLE_PROTECT_ALLIES && !within_two_tiles) {
        limit = 2;
    }
    if (ai->acting_unit_decision->targeting_flags_1 & AI_TARGETING_FLAG_1_REFLECTED_ACTION) {
        destination_select(0x7fffffff, AI_DESTINATION_PRIORITY_FIRST);
    } else {
        destination_with_nearest_fallback_select(limit);
    }
    ai->movement_scenario = 1;
    ai->candidate_coords[1] = ai->candidate_coords[0];
    if (!target_distance_tiles_filter(range)) {
        candidate_actor_reachability_set();
    }
    ai->movement_scenario = 2;
    ai_mem_copy_u16_values(ai->reachable_tiles[2], ai->reachable_tiles[0], sizeof(ai->reachable_tiles[0]));
    if (!ai->acting_unit->action_taken) {
        s32 closer_distance = distance - 1;
        if (closer_distance < limit) {
            limit = closer_distance;
        }
        destination_with_range_floor_select(limit);
    } else {
        ai->candidate_coords[2] = ai->candidate_coords[1];
    }
    ai->reachable_tiles[1][ai->acting_unit_coords.fields.layer][ai->acting_unit_coords.fields.y]
        |= 0x8000 >> ai->acting_unit_coords.fields.x;
    return 0;
}

/* Select an action for the recovery (protect allies) auto-battle mode: move toward the
 * focus target, try to cancel its listed statuses, then rank abilities on it, on enemies
 * and on allies before the fallback. Returns -1 to suspend, else 0 (never -2). */
static s32 protect_allies_select(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* acting = g_ai_workspace_storage.acting_unit;
    u8 statuses[AI_PROTECT_ALLIES_STATUS_COUNT] = { STATUS_ID_DEAD, STATUS_ID_PETRIFY, STATUS_ID_BLOOD_SUCK };
    s32 target;
    u32 status;
    s32 result;

    target = acting->auto_battle_target;
    if (g_ai_workspace_storage.resume_pending) {
        /* Resume: each phase re-enters the step that suspended. */
        switch (g_ai_workspace_storage.action_selection_phase) {
        case AI_PROTECT_ALLIES_SELECTION_SCORE_TILES:
            goto score_tiles;
        case AI_PROTECT_ALLIES_SELECTION_MOVEMENT:
            goto movement;
        case AI_PROTECT_ALLIES_SELECTION_CHARGING:
            goto charging;
        case AI_PROTECT_ALLIES_SELECTION_TARGET_ABILITIES:
            goto target_abilities;
        case AI_PROTECT_ALLIES_SELECTION_ENEMY_ABILITIES:
            goto enemy_abilities;
        case AI_PROTECT_ALLIES_SELECTION_CANCEL_STATUS:
            goto cancel_status;
        default:
            goto ally_abilities;
        }
    }
    g_ai_workspace_storage.movement_scenario = 0;
score_tiles:
    if (reachable_tiles_score() == -1) {
        ai->action_selection_phase = AI_PROTECT_ALLIES_SELECTION_SCORE_TILES;
        return -1;
    }
    g_protect_allies_status_retry = 0;
    if (((ai->unit_decisions[target].targeting_flags_2 >> 4) & (AI_TARGET_UNTARGETABLE >> 4))
        || ai->unit_decisions[target].enemy_flag) {
    retarget:
        target_in_peril_select();
        target = ai->acting_unit->auto_battle_target;
    }
    decision_target_focus(target);
movement:
    if (targeted_build() == -1) {
        ai->action_selection_phase = AI_PROTECT_ALLIES_SELECTION_MOVEMENT;
        return -1;
    }
    ai->movement_scenario = 2;
    rank_baseline_seed();
    ai->movement_scenario = 0;
    if (!g_protect_allies_status_retry
        && !((ai->unit_decisions[target].targeting_flags_2 >> 6) & (AI_TARGET_DEAD_WITH_RERAISE >> 6))) {
        g_protect_allies_status_index = 0;
        do {
            u32 status_flags
                = g_unit_stats[target].status_sets.current[(status = statuses[g_protect_allies_status_index]) >> 3];
            if (status_flags & (0x80 >> (status & 7))) {
            cancel_status:
                result = status_cancellation_evaluate(target, statuses[g_protect_allies_status_index]);
                if (result == -1) {
                    ai->action_selection_phase = AI_PROTECT_ALLIES_SELECTION_CANCEL_STATUS;
                    return -1;
                }
                if (result == 2)
                    return 0;
                if (result == 0) {
                    /* Retry target selection once, then skip further
                     * status-cancel attempts; the retry re-enters the
                     * retarget block above. */
                    g_protect_allies_status_retry = 1;
                    goto retarget;
                }
            }
            g_protect_allies_status_index++;
        } while (g_protect_allies_status_index < AI_PROTECT_ALLIES_STATUS_COUNT);
    }
    ai->movement_scenario = 1;
charging:
    result = keep_charging_select();
    switch (result) {
    case -1:
        ai->action_selection_phase = AI_PROTECT_ALLIES_SELECTION_CHARGING;
        return -1;
    case 1:
        return 0;
    }
    ai->movement_scenario = 0;
    if (target_active_units_select(AI_UNIT_FILTER_SPECIFIC, target)) {
    target_abilities:
        if (ability_list_evaluate() == -1) {
            ai->action_selection_phase = AI_PROTECT_ALLIES_SELECTION_TARGET_ABILITIES;
            return -1;
        }
        if (ranked_select())
            return 0;
    }
    ai->movement_scenario = 1;
    if (target_active_units_select(AI_UNIT_FILTER_ENEMIES, 0)) {
    enemy_abilities:
        if (ability_list_evaluate() == -1) {
            ai->action_selection_phase = AI_PROTECT_ALLIES_SELECTION_ENEMY_ABILITIES;
            return -1;
        }
        if (ranked_select())
            return 0;
    }
    if (target_active_units_select(AI_UNIT_FILTER_ALLIES, 0)) {
    ally_abilities:
        if (ability_list_evaluate() != -1) {
            if (ranked_select())
                return 0;
        } else {
            ai->action_selection_phase = AI_PROTECT_ALLIES_SELECTION_ALLY_ABILITIES;
            return -1;
        }
    }
    ai->movement_scenario = 2;
    fallback_select();
    return 0;
}

/* Evaluate the actor's abilities for removing one status from a target.
 *
 * The return value distinguishes suspension (-1), no usable ability (0), a
 * retry-worthy result (1), and a selected action (2). The Blood Suck path
 * deliberately tests the transient Jump flag, preserving the shipped AI
 * quirk.
 */
static s32 status_cancellation_evaluate(s32 unit_id, s32 status_id) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* unit = &g_unit_stats[unit_id];
    ai_unit_decision_t* decision;
    ai_ability_entry_t* entry;
    s32 flags;
    s32 status_byte;

    if (g_ai_workspace_storage.resume_pending != 0) {
        /* Resume: the target jumps into the middle of the ability loop. */
        goto evaluate_ability;
    }

    decision = &ai->unit_decisions[unit_id];
    g_status_candidate_count = 0;
    g_status_insufficient_mp = 0;
    g_status_sufficient_mp = 0;

    switch (status_id) {
    case STATUS_ID_DEAD:
        if (!(decision->flags & AI_DECISION_TRANSIENT_DEAD))
            return 1;
        break;
    case STATUS_ID_PETRIFY:
        if (!(decision->flags & AI_DECISION_TRANSIENT_PETRIFIED))
            return 1;
        break;
    case STATUS_ID_BLOOD_SUCK:
        if (!(decision->flags & AI_DECISION_TRANSIENT_JUMP))
            return 1;
        break;
    }

    if (target_active_units_select(AI_UNIT_FILTER_SPECIFIC, unit_id) != 0) {
        for (ai->ability_counter = 0; ai->ability_counter < AI_ABILITY_LIST_COUNT; ai->ability_counter++) {
            entry = &ai->ability_lists[ai->acting_unit_compact_index][ai->ability_counter];
            if (entry->skillset == SKILLSET_ID_NONE)
                break;
            ai_ability_entry_load(entry);
            flags = ai->considered_ability.ai_flags.word;
            if (!(flags & AI_ABILITY_CANCEL_STATUS))
                continue;

            status_byte = status_id / 8;
            if (!(ai->considered_ability.parameters.fields.status_infliction[status_byte] & (0x80 >> (status_id % 8))))
                continue;
            if ((flags & AI_ABILITY_AFFECTED_BY_FAITH)
                && (unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
                    & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)))
                continue;
            if ((flags & AI_ABILITY_REFLECTABLE)
                && (unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_REFLECT)]
                    & STATUS_BYTE_MASK(STATUS_ID_REFLECT)))
                continue;
            if ((flags & AI_ABILITY_AFFECTED_BY_SILENCE)
                && (ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)]
                    & STATUS_BYTE_MASK(STATUS_ID_SILENCE)))
                continue;

            if (ai->acting_unit->mp < ai->considered_ability.mp_cost)
                g_status_insufficient_mp = 1;
            else
                g_status_sufficient_mp = 1;
            g_status_candidate_count++;

        evaluate_ability:
            if (ai_ability_outcome_evaluate() == -1)
                return -1;
        }
    }

    if (ai->ranked_actions[0].rank.base_hit_percent != 0) {
        ai_mem_copy_u16_values(&ai->selected_action, &ai->ranked_actions[0], sizeof(ai_action_data_t));
        return 2;
    }
    if (g_status_candidate_count == 0)
        return 0;
    if (g_status_insufficient_mp != 0 && ai->has_mp_recovery_ability == 0 && g_status_sufficient_mp == 0)
        return 0;
    return 1;
}

/*
 * Select the allied target with the highest recovery-priority score.
 *
 * Keep the actor as fallback and retain the earlier slot on equal scores.
 * Status-helper checks require the actor's aggregate cancellation capability;
 * the HP-recovery flag enables dead-with-Reraise, Critical and low-HP scoring.
 * Return 1 only when the best signed score exceeds 0x02000000. Only a status
 * score (at least 0x72 << 24) can; an HP-only score stays below 129 << 16. So
 * the result is 1 exactly when some ally has one of the scored conditions.
 */
static s32 target_in_peril_select(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 best_score = 0;
    s32 i;

    g_ai_workspace_storage.acting_unit->auto_battle_target = g_ai_workspace_storage.acting_unit_id;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit_stats_t* unit = &g_unit_stats[i];
        ai_unit_decision_t* decision = &ai->unit_decisions[i];
        s32 distance_score, missing_hp, team_bits, status_score, score;

        if (decision->targeting_flags_2 & AI_TARGET_UNTARGETABLE) {
            continue;
        }
        if (decision->enemy_flag) {
            continue;
        }
        distance_score = 255 - ai_unit_distance_calculate(ai->acting_unit, unit);
        missing_hp = 128 - ((unit->hp << 7) / unit->max_hp);
        team_bits = unit->team_flags & UNIT_TEAM_IMPORTANCE_MASK;
        if (status_can_cancel_current(unit, STATUS_ID_DEAD)
            && (decision->targeting_flags_2 & AI_TARGET_DEAD_WITHOUT_RERAISE)) {
            status_score = 0x78;
        } else if ((ai->acting_unit_decision->targeting_flags_1 & AI_TARGETING_FLAG_1_CAN_HEAL_ALLIES)
            && ((decision->targeting_flags_2 & AI_TARGET_DEAD_WITH_RERAISE)
                || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)]
                    & STATUS_BYTE_MASK(STATUS_ID_CRITICAL)))) {
            status_score = 0x77;
        } else if (status_can_cancel_current(unit, STATUS_ID_BLOOD_SUCK)) {
            status_score = 0x76;
        } else if (status_can_cancel_current(unit, STATUS_ID_PETRIFY)) {
            status_score = 0x75;
        } else if (status_can_cancel_current(unit, STATUS_ID_CONFUSION)) {
            status_score = 0x74;
        } else if (status_can_cancel_current(unit, STATUS_ID_FROG)) {
            status_score = 0x73;
        } else {
            status_score = 0;
            if (ai->acting_unit_decision->targeting_flags_1 & AI_TARGETING_FLAG_1_CAN_HEAL_ALLIES) {
                status_score = (decision->targeting_flags_2 & AI_TARGET_HP_BELOW_HALF) != 0 ? 0x72 : 0;
            }
        }
        if (status_score == 0) {
            if (decision->targeting_flags_1 & AI_TARGETING_FLAG_1_MOSTLY_SUPPORT) {
                continue;
            }
            if (ai->acting_unit_id == i) {
                distance_score = 0;
            }
            score = (missing_hp << 16) + (team_bits << 8) + distance_score;
        } else {
            score = (status_score << 24) + (distance_score << 16) + (missing_hp << 8) + team_bits;
        }
        if (best_score < score) {
            best_score = score;
            ai->acting_unit->auto_battle_target = i;
        }
    }
    /* Clear the unit's special targeting behavior; this does not impose a distance limit. */
    ai->acting_unit_decision->flags &= ~AI_DECISION_SPECIAL_BEHAVIOR;
    return best_score > 0x02000000;
}

/*
 * Test whether the actor's available abilities can remove this current status.
 *
 * Innate statuses are excluded. The cancellation mask aggregates the actor's
 * usable ability list; it does not describe only the considered ability.
 */
static s32 status_can_cancel_current(const unit_stats_t* unit, s32 status_bit) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 byte_off;
    s32 bit;
    s32 mask;

    byte_off = status_bit / 8;
    bit = status_bit - byte_off * 8;
    mask = 0x80;

    if (!(unit->status_sets.current[byte_off] & (mask >> bit))) {
        return 0;
    }
    if (unit->status_sets.innate[byte_off] & (mask >> bit)) {
        return 0;
    }
    if (!(ai->status_to_cancel[byte_off] & (mask >> bit))) {
        return 0;
    }
    return 1;
}

/* With an ally in peril, switch to the recovery policy and return -2.
 * Otherwise focus on the unit's auto-battle target and run the general search;
 * -1 suspends. */
static s32 save_fading_life_select(void) {
    if (g_ai_workspace_storage.resume_pending == 0) {
        if (target_in_peril_select() != 0) {
            g_ai_workspace_storage.autobattle_setting = AI_AUTOBATTLE_PROTECT_ALLIES;
            return -2;
        }
        decision_target_focus(g_ai_workspace_storage.acting_unit->auto_battle_target);
    }
    if (general_select() == -1)
        return -1;
    return 0;
}

/*
 * Select an action for the retreat-oriented AI path.
 *
 * Focus on the nearest healer or crystal and pick a cowardly destination, then
 * try abilities targeting the actor and an existing charge. Charge-skillset
 * abilities are then marked unusable before the pass over all units, and the
 * default action is the fallback. Used for the RETREAT auto-battle mode, for a
 * Critical or Frog unit when retreat support is available, and for a Don't
 * Act unit with a focus target. Return -1 to suspend and 0 when selection
 * completes.
 */
static s32 retreat_select(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    ai_ability_entry_t* entry;
    s32 i, result;

    if (g_ai_workspace_storage.resume_pending) {
        /* Resume: re-enter the step that suspended. */
        switch (g_retreat_policy_stage) {
        case AI_RETREAT_MOVEMENT:
            goto movement;
        case AI_RETREAT_SELF_ABILITIES:
            goto self_abilities;
        case AI_RETREAT_CHARGING:
            goto charging;
        default:
            goto other_abilities;
        }
    }
    decision_target_focus(ai_target_find_nearest(AI_NEAREST_HEALER_OR_CRYSTAL));
movement:
    if (retreat_build() == -1) {
        g_retreat_policy_stage = AI_RETREAT_MOVEMENT;
        return -1;
    }
    ai->movement_scenario = 1;
    rank_baseline_seed();
    if (target_active_units_select(AI_UNIT_FILTER_SPECIFIC, ai->acting_unit_id)) {
    self_abilities:
        if (ability_list_evaluate() == -1) {
            g_retreat_policy_stage = AI_RETREAT_SELF_ABILITIES;
            return -1;
        }
    }
    if (ranked_select())
        return 0;
charging:
    result = keep_charging_select();
    switch (result) {
    case -1:
        g_retreat_policy_stage = AI_RETREAT_CHARGING;
        return -1;
    case 1:
        return 0;
    }
    for (i = 0; i < AI_ABILITY_LIST_COUNT; i++) {
        entry = &ai->ability_lists[ai->acting_unit_compact_index][i];
        if (entry->skillset == SKILLSET_ID_NONE)
            break;
        ai_ability_entry_load(entry);
        /* The aligned skillset/usage pair preserves the target's LHU/SH. */
        if (g_command_menu_types_by_skillset[entry->skillset] == ACTION_MENU_TYPE_CHARGE)
            entry->usage.policy.usable = 0;
    }
    if (target_active_units_select(AI_UNIT_FILTER_ANY, 255)) {
    other_abilities:
        if (ability_list_evaluate() == -1) {
            g_retreat_policy_stage = AI_RETREAT_OTHER_ABILITIES;
            return -1;
        }
        if (ranked_select())
            return 0;
    }
    fallback_select();
    return 0;
}

/* Score the reachable tiles, fill the distance grid for the decision's target,
 * and store the best destination as movement scenario 1. -1 suspends; the next
 * call resumes at the step that did. */
static s32 retreat_build(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    ai_move_retreat_stage_e state;

    if (g_ai_workspace_storage.resume_pending != 0) {
        /* Resume: re-enter the step that suspended. */
        state = g_retreat_move_stage;
        if (state == AI_MOVE_RETREAT_SCORE_TILES) {
            goto score_tiles;
        }
        if (state == AI_MOVE_RETREAT_TARGET_DISTANCE) {
            goto fill_target_distance;
        }
    }
    g_ai_workspace_storage.movement_scenario = 0;
score_tiles:
    if (reachable_tiles_score() == -1) {
        g_retreat_move_stage = AI_MOVE_RETREAT_SCORE_TILES;
        return -1;
    }
fill_target_distance:
    if (target_distance_grid_fill(&ai->acting_unit_decision->target) == -1) {
        g_retreat_move_stage = AI_MOVE_RETREAT_TARGET_DISTANCE;
        return -1;
    }
    destination_select(0xff, AI_DESTINATION_PRIORITY_FIRST);
    ai->movement_scenario = 1;
    ai->candidate_coords[1] = ai->candidate_coords[0];
    candidate_actor_reachability_set();
    return 0;
}

/*
 * Select the nearest unit admitted by the requested target filter.
 *
 * Manhattan-distance ties keep the first slot. If none qualify, return the
 * acting unit's ID. Every mode skips untargetable and petrified units, and all
 * but AI_NEAREST_ELIGIBLE_ENEMY dead ones; AI_NEAREST_HEALER_OR_CRYSTAL admits
 * crystals before those checks, and AI_NEAREST_ELIGIBLE_ENEMY falls through to
 * the Blood Suck exclusion of AI_NEAREST_WITHOUT_BLOOD_SUCK.
 */
s32 ai_target_find_nearest(ai_nearest_target_mode_e mode) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 nearest_distance = 0x7fffffff;
    s32 nearest_id = g_ai_workspace_storage.acting_unit_id;
    s32 i;
    unit_stats_t* unit;
    ai_unit_decision_t* decision;
    s32 distance;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = &g_unit_stats[i];
        decision = &ai->unit_decisions[i];
        /* Crystals always qualify as healer-mode destinations. */
        if (mode != AI_NEAREST_HEALER_OR_CRYSTAL
            || !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))) {
            if (ai_target_is_untargetable(i))
                continue;
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
                continue;
            if (mode != AI_NEAREST_ELIGIBLE_ENEMY
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)))
                continue;
            switch (mode) {
            case AI_NEAREST_HEALER_OR_CRYSTAL:
                if (decision->enemy_flag || !(decision->targeting_flags_1 & AI_TARGETING_FLAG_1_CAN_HEAL_ALLIES))
                    continue;
                break;
            case AI_NEAREST_IMMINENT_ENEMY:
                if (!ai->targetability.live.unit_targetable[i])
                    continue;
                if (target_is_excluded_from_imminent_enemies(i))
                    continue;
                if (ai_unit_distance_calculate(ai->acting_unit, unit) < 2
                    && ai_height_difference_calculate(&ai->acting_unit_coords, unit) < -2)
                    continue;
                break;
            case AI_NEAREST_ENEMY:
                if (!decision->enemy_flag)
                    continue;
                break;
            case AI_NEAREST_ELIGIBLE_ENEMY:
                if (!decision->enemy_flag || (decision->targeting_flags_2 & AI_TARGET_DEAD_WITHOUT_RERAISE))
                    continue;
                if (!ai->targetability.live.unit_targetable[i] || !(decision->flags & AI_DECISION_CONSIDERED))
                    continue;
            case AI_NEAREST_WITHOUT_BLOOD_SUCK:
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_BLOOD_SUCK)]
                    & STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK))
                    continue;
                break;
            }
        }
        distance = ai_unit_distance_calculate(ai->acting_unit, unit);
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest_id = i;
        }
    }
    return nearest_id;
}

/* Distance in tiles along the map axes (|dx| + |dy|), ignoring height. */
s32 ai_unit_distance_calculate(const unit_stats_t* first, const unit_stats_t* second) {
    s32 dx = first->x - second->x;
    s32 dy = first->position.y - second->position.y;
    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    return dx + dy;
}

/*
 * Make only the candidate and acting tiles reachable in the selected scenario.
 *
 * Clear both 18-row levels before setting the candidate bit and adding the
 * acting bit. Integer address stages preserve the target's addition order;
 * direct typed indexing introduces extra instructions. Field offsets come
 * from the shared workspace declaration.
 */
static void candidate_actor_reachability_set(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 row;

    for (row = 0; row < AI_GRID_ROW_COUNT; row++) {
        u32 row_address = row * sizeof(u16);
        u32 scenario_address = (u32)ai + ai->movement_scenario * sizeof(ai->reachable_tiles[0]);
        *(u16*)(row_address + scenario_address + offsetof(ai_workspace_t, reachable_tiles[0][0][0])) = 0;
        scenario_address = (u32)ai + ai->movement_scenario * sizeof(ai->reachable_tiles[0]);
        row_address += scenario_address;
        *(u16*)(row_address + offsetof(ai_workspace_t, reachable_tiles[0][1][0])) = 0;
    }
    {
        u32 scenario_address = ai->movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
        u32 coords_address = (u32)ai + ai->movement_scenario * sizeof(ai->candidate_coords[0]);
        u32 level_address;
        u32 row_address;
        scenario_address += offsetof(ai_workspace_t, reachable_tiles);
        level_address = *(u8*)(coords_address + offsetof(ai_workspace_t, candidate_coords[0].fields.layer))
                * sizeof(ai->reachable_tiles[0][0])
            + scenario_address;
        row_address = *(u8*)(coords_address + offsetof(ai_workspace_t, candidate_coords[0].fields.y))
                * sizeof(ai->reachable_tiles[0][0][0])
            + level_address;
        *(u16*)row_address = 0x8000 >> *(u8*)(coords_address + offsetof(ai_workspace_t, candidate_coords[0].fields.x));
    }
    {
        u32 scenario_address = ai->movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
        u32 level_address;
        u32 row_address;
        scenario_address += offsetof(ai_workspace_t, reachable_tiles);
        level_address = ai->acting_unit_coords.fields.layer * sizeof(ai->reachable_tiles[0][0]) + scenario_address;
        row_address = ai->acting_unit_coords.fields.y * sizeof(ai->reachable_tiles[0][0][0]) + level_address;
        *(u16*)row_address |= 0x8000 >> ai->acting_unit_coords.fields.x;
    }
}

/* Fall back to moving to the current scenario's candidate tile, then Defending
 * when the unit's support ability is Defend, else taking no action. */
static void fallback_select(void) {
    u8 scenario = g_ai_workspace_storage.movement_scenario;
    unit_stats_t* unit = g_ai_workspace_storage.acting_unit;
    u32 coords;
    s32 support;

    g_ai_workspace_storage.selected_action.act_before_move = 0;
    coords = g_ai_workspace_storage.candidate_coords[scenario].word;
    support = unit->abilities.support;
    g_ai_workspace_storage.selected_action.coords.word = coords;
    if (support == ABILITY_ID_SUPPORT_DEFEND) {
        g_ai_workspace_storage.selected_action.command.skillset_id = SKILLSET_ID_DEFEND;
    } else {
        g_ai_workspace_storage.selected_action.command.skillset_id = 0;
    }
}

/*
 * Fill target-distance estimates for rows containing reachable tiles.
 *
 * Unmounted Teleport or Teleport 2 uses Manhattan distance; other movement subtracts
 * propagated remaining-range values. Fill every map-width tile in each
 * selected row, leaving zero-mask rows unchanged. Return -1 when the child
 * suspends through its out-parameter, or 0 after filling the grid.
 */
static s32 target_distance_grid_fill(const ai_coords_t* target) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 suspended = 0;
    s32 use_manhattan;
    s32 target_remaining_range;
    s32 level, x, y;
    s32 distance, y_distance;

    if (g_ai_workspace_storage.resume_pending != 0)
        g_ai_workspace_storage.search_resume_flag = 0;
    else
        g_ai_workspace_storage.search_resume_flag = 1;
    use_manhattan = 0;
    if (!(ai->acting_unit->mount_info & UNIT_MOUNT_INFO_FLAG_RIDER)
        && (ai->acting_unit->ability_flags.movement[1]
            & (UNIT_MOVEMENT_SET_2_TELEPORT | UNIT_MOVEMENT_SET_2_TELEPORT_2))) {
        use_manhattan = 1;
    } else {
        ai_move_propagate_from_target(ai->acting_unit_id, target->fields.x, target->fields.y, target->fields.layer,
            ai->search_resume_flag, &suspended, 1);
        if (suspended != 0)
            return -1;
        ai->resume_pending = 0;
        target_remaining_range = g_target_panels[(target->fields.layer * BATTLE_MAP_TILES_PER_LAYER)
            + target->fields.y * ai->terrain_width + target->fields.x]
                                     .remaining_range;
    }
    for (level = 0; level < BATTLE_MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->terrain_depth; y++) {
            if (ai->reachable_tiles[0][level][y] != 0) {
                for (x = 0; x < ai->terrain_width; x++) {
                    if (use_manhattan) {
                        distance = target->fields.x - x;
                        if (distance < 0)
                            distance = -distance;
                        y_distance = target->fields.y - y;
                        if (y_distance < 0)
                            y_distance = -y_distance;
                        distance += y_distance;
                    } else {
                        distance = target_remaining_range
                            - g_target_panels[(level * BATTLE_MAP_TILES_PER_LAYER) + y * ai->terrain_width + x]
                                  .remaining_range;
                    }
                    ai->tile_target_distance[level][y][x] = distance;
                }
            }
        }
    }
    return 0;
}

/*
 * Score reachable tiles by nearby eligible enemies.
 *
 * Each enemy contributes max(10 - Manhattan distance, 0), ignoring height.
 * Only scenario-0 reachable tiles are updated; other cells remain unchanged.
 * The shared targetability list is rebuilt before scoring both map layers.
 */
static void foe_proximity_grid_fill(void) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 i;
    s32 level;
    s32 y;
    s32 x;
    s32 distance;
    s32 dy;
    u16 row;
    unit_stats_t* unit;

    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        ai->targetability.live.unit_targetable[i] = 0;
        if (target_is_out_of_play(i) == 0 && target_is_excluded_from_imminent_enemies(i) == 0) {
            ai->targetability.live.unit_targetable[i] = 1;
        }
    }
    for (level = 0; level < BATTLE_MAP_TERRAIN_LAYER_COUNT; level++) {
        unit_stats_t* units = g_unit_stats;
        s32 proximity_limit = 10;
        for (y = 0; y < ai->terrain_depth; y++) {
            row = ai->reachable_tiles[0][level][y];
            if (row == 0) {
                continue;
            }
            for (x = 0; x < ai->terrain_width; x++) {
                if (!((row << x) & 0x8000)) {
                    continue;
                }
                ai->tile_foe_proximity[level][y][x] = 0;
                for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                    unit = &units[i];
                    if (ai->targetability.live.unit_targetable[i] == 0) {
                        continue;
                    }
                    distance = unit->x - x;
                    if (distance < 0) {
                        distance = -distance;
                    }
                    dy = unit->position.y - y;
                    if (dy < 0) {
                        dy = -dy;
                    }
                    distance += dy;
                    if (distance < 11) {
                        distance = proximity_limit - distance;
                        ai->tile_foe_proximity[level][y][x] += distance;
                    }
                }
            }
        }
    }
}

/* Rejects a unit from the AI's imminent-enemy set.
 *
 * The unit must be an enemy that acts no later than the acting unit and will
 * no longer be disabled by Stop, Don't Act, Sleep, Chicken, or Charm then. */
static s32 target_is_excluded_from_imminent_enemies(s32 unit_id) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    unit_stats_t* unit;
    s32 limit;

    if (g_ai_workspace_storage.unit_decisions[unit_id].enemy_flag == 0)
        return 1;
    unit = &g_unit_stats[unit_id];
    limit = ai->acting_unit_remaining_clockticks;
    if (limit < ai_unit_turn_delay_calculate(unit)
        || ai_status_is_active_through_delay(ai->acting_unit_remaining_clockticks, unit, STATUS_ID_STOP)
        || ai_status_is_active_through_delay(ai->acting_unit_remaining_clockticks, unit, STATUS_ID_DONT_ACT)
        || ai_status_is_active_through_delay(ai->acting_unit_remaining_clockticks, unit, STATUS_ID_SLEEP)
        || ai_status_is_active_through_delay(ai->acting_unit_remaining_clockticks, unit, STATUS_ID_CHICKEN))
        return 1;
    return ai_status_is_active_through_delay(ai->acting_unit_remaining_clockticks, unit, STATUS_ID_CHARM) != 0;
}

/*
 * Score each reachable tile, resuming suspended evaluations.
 *
 * Return -1 to suspend and 0 after restoring the simulation coordinates.
 * Integer address stages preserve the target's addition order; direct typed
 * indexing changes the machine code. Each level has 18 rows of 16 tiles.
 */
static s32 reachable_tiles_score(void) {
    s32 x, y, level;
    ai_workspace_t* ai = &g_ai_workspace_storage;
    u32 row;

    if (g_ai_workspace_storage.resume_pending != 0) {
        x = g_ai_workspace_storage.current_action.coords.fields.x;
        y = g_ai_workspace_storage.current_action.coords.fields.y;
        level = g_ai_workspace_storage.current_action.coords.fields.layer;
        {
            u32 scenario_address = g_ai_workspace_storage.movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
            u32 level_address;
            u32 row_address;
            scenario_address += offsetof(ai_workspace_t, reachable_tiles);
            level_address = level * sizeof(ai->reachable_tiles[0][0]) + scenario_address;
            row_address = y * sizeof(ai->reachable_tiles[0][0][0]) + level_address;
            row = *(u16*)row_address;
        }
        /* This tile was already set up before the interrupted evaluation. */
        goto evaluate_tile;
    }
    level = 0;
    g_ai_workspace_storage.current_action.act_before_move = 0;
    g_ai_workspace_storage.current_action.command.skillset_id = 0;
    for (; level < BATTLE_MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->terrain_depth; y++) {
            {
                u32 scenario_address = ai->movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
                u32 level_address;
                u32 row_address;
                scenario_address += offsetof(ai_workspace_t, reachable_tiles);
                level_address = level * sizeof(ai->reachable_tiles[0][0]) + scenario_address;
                row_address = y * sizeof(ai->reachable_tiles[0][0][0]) + level_address;
                row = *(u16*)row_address;
            }
            if (row != 0) {
                for (x = 0; x < ai->terrain_width; x++) {
                    if ((row << x) & 0x8000) {
                        ai->current_action.coords.fields.x = x;
                        ai->current_action.coords.fields.y = y;
                        ai->current_action.coords.fields.layer = level;
                        ai_tile_pickup_update();
                    evaluate_tile:
                        if (ai_action_simulate_and_score() == -1) {
                            /* Keep the current coordinates and uncommitted score. */
                            return -1;
                        }
                        {
                            u32 level_address = level * sizeof(ai->tile_priority[0]) + (u32)ai;
                            u32 row_address;
                            u32 tile_address;
                            level_address += offsetof(ai_workspace_t, tile_priority);
                            row_address = y * sizeof(ai->tile_priority[0][0]) + level_address;
                            tile_address = x * sizeof(ai->tile_priority[0][0][0]) + row_address;
                            *(u16*)tile_address = -ai->current_action.rank.priority;
                        }
                    }
                }
            }
        }
    }
    /* Restore the simulation unit's coordinates only after the whole scan completes. */
    ai_acting_unit_place(&ai->acting_unit_coords);
    return 0;
}

/* Clockticks until the unit gets a turn: its Stop CT plus (100 - CT) / Speed
 * rounded up, and no less than its Sleep CT while it sleeps. 0x7fffffff when
 * it never will: Speed 0, crystal, petrified, treasure, or dead without
 * Reraise (from its decision record). Don't Act and Don't Move are not looked
 * at. */
s32 ai_unit_turn_delay_calculate(const unit_stats_t* unit) {
    ai_workspace_t* ai = &g_ai_workspace_storage;
    s32 speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    s32 ticks = 0;
    s32 rem;

    if (speed == 0) {
        return 0x7FFFFFFF;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return 0x7FFFFFFF;
    }
    if ((ai->unit_decisions[unit->active_battle_id].targeting_flags_2 >> 5) & (AI_TARGET_DEAD_WITHOUT_RERAISE >> 5)) {
        return 0x7FFFFFFF;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_STOP)] & STATUS_BYTE_MASK(STATUS_ID_STOP)) {
        ticks = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_STOP)];
    }
    if (unit->ct < UNIT_TURN_CT_THRESHOLD) {
        rem = UNIT_TURN_CT_THRESHOLD - unit->ct;
        ticks += rem / speed;
        if (rem % speed != 0) {
            ticks += 1;
        }
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) {
        return ticks;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)] & STATUS_BYTE_MASK(STATUS_ID_SLEEP)) == 0) {
        return ticks;
    }
    if (ticks < unit->status_ct[STATUS_CT_INDEX(STATUS_ID_SLEEP)]) {
        ticks = unit->status_ct[STATUS_CT_INDEX(STATUS_ID_SLEEP)];
    }
    return ticks;
}

/*
 * Predict the clockticks remaining before the death counter expires.
 *
 * Divide (counter + 1) * 100 - CT by speed, adding one for any remainder.
 * Counter zero lasts another turn; expiration may revive an Undead unit.
 * Callers reject an ability only when its CT is greater than this result.
 * The final two parameters are overwritten before use. Keeping them as
 * argument-register scratch reproduces the target's a1/a2 allocation; callers
 * supply only the unit pointer.
 */
s32 ai_unit_death_delay_calculate(unit_stats_t* unit, s32 total, s32 speed) {
    s32 counter;
    s32 ticks;
    s32 sum;

    speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    if (speed == 0)
        return 0x7fffffff;
    if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)))
        return 0x7fffffff;
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)] & STATUS_BYTE_MASK(STATUS_ID_RERAISE))
        return 0x7fffffff;
    counter = unit->death_counter;
    if (counter == 0xff)
        return 0x7fffffff;
    total = counter;
    total *= UNIT_TURN_CT_THRESHOLD;
    sum = total + UNIT_TURN_CT_THRESHOLD;
    total = sum - unit->ct;
    ticks = total / speed;
    if (total % speed)
        ticks++;
    return ticks;
}
