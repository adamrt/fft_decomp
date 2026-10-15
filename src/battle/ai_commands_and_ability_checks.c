/* Ai commands and ability checks. Its start is a boundary the binary proves; its end is a cut chosen by topic. */

#include "fft/battle_ai.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/libetc.h"
#include "psx/types.h"

/*
 * Prepare the next AI movement, action or end-turn command.
 *
 * Resume suspended setup stages before resolving the selected action. Return
 * -1 while suspended, or 0 after writing a command. Early end-turn paths skip
 * coordinate restoration and command-history updates.
 */
s32 ai_build_command(s32 unit_id, ai_command_t* command) {
    ai_data_t* ai;
    unit_stats_t* unit;
    ai_command_t* last_command;
    ai_command_t* history;
    s32 i;
    s32 ticks;
    u8 moved;

    ticks = VSync(1);
    ai = &g_ai_data_base;
    if (ticks >= AI_DECISION_SCANLINE_LIMIT)
        return -1;
    if (g_ai_data_base.resume_pending) {
        /* Resume: re-enter the suspended decision stage. */
        switch (g_ai_data_base.main_ai_state) {
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
        command->kind = AI_COMMAND_END_TURN;
        return 0;
    }
    ai->acting_unit_id = unit_id;
    if (ai->unit_compact_indices[unit_id] >= AI_COMPACT_UNIT_COUNT) {
        command->kind = AI_COMMAND_END_TURN;
        return 0;
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (g_battle_unit_stats[i].entd_slot != BATTLE_ENTD_SLOT_NONE
            && ai->unit_compact_indices[i] >= AI_COMPACT_UNIT_COUNT) {
            command->kind = AI_COMMAND_END_TURN;
            return 0;
        }
    }
    ai_init_acting_unit_data();
    if (ai->acting_unit->charged_ability_ct == 0xff)
        ai->acting_unit_decision->targeting_flags_1 &= ~AI_TARGETING_FLAG_1_REFLECTED_ACTION;
    if (!ai->selected_action.rank.decided || ai->acting_unit->action_taken) {
        ai_begin_action_simulation();
    refresh:
        if (ai_refresh_unit_decision_flags() == -1) {
            ai->main_ai_state = AI_COMMAND_BUILD_REFRESH;
            ai->resume_pending = 1;
            return -1;
        }
        ai_set_ability_considerations(ai->acting_unit->action_taken);
    movement:
        if (ai_set_movement_panel_data(ai->acting_unit->movement_taken) == -1) {
            ai->main_ai_state = AI_COMMAND_BUILD_MOVEMENT;
            ai->resume_pending = 1;
            return -1;
        }
        ai->resume_pending = 0;
        ai_update_team_targeting_flags();
        ai_init_target_consideration();
    choose:
        if (ai_select_action_initial() == -1) {
            ai->main_ai_state = AI_COMMAND_BUILD_CHOOSE_ACTION;
            ai->resume_pending = 1;
            return -1;
        }
        ai->resume_pending = 0;
        ai->selected_action.rank.decided = AI_ACTION_DECIDED;
        if (ai->selected_action.reflected_action)
            ai->acting_unit_decision->targeting_flags_1 |= AI_TARGETING_FLAG_1_REFLECTED_ACTION;
        ai_end_action_simulation();
    }
    if (!ai->acting_unit->movement_taken && (ai->acting_unit->action_taken || !ai->selected_action.act_before_move)) {
        ai->selected_action.coords.fields.zero = 0;
        if (ai->selected_action.coords.word != ai->acting_unit_coords.word) {
            command->kind = AI_COMMAND_MOVE;
            command->data.move.x = ai->selected_action.coords.fields.x;
            command->data.move.y = ai->selected_action.coords.fields.y;
            command->data.move.layer = ai->selected_action.coords.fields.elevation;
            goto preserve;
        }
    }
    if (ai->acting_unit->action_taken || !ai->selected_action.command.skillset_id) {
        command->kind = AI_COMMAND_END_TURN;
    } else {
        command->kind = AI_COMMAND_ACT;
        mem_copy_u16_values(&command->data.action, &ai->selected_action.command, sizeof(command->data.action));
    }
preserve:
    moved = ai->acting_unit->movement_taken;
    ai->movement_taken = moved;
    unit = ai->acting_unit;
    ai->action_taken = unit->action_taken;
    ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    history = &g_ai_command_history[0];
    last_command = history + 1;
    mem_copy_u16_values(history, last_command, sizeof(*history));
    mem_copy_u16_values(last_command, command, sizeof(*last_command));
    return 0;
}

/*
 * Populate the AI block acting-unit fields for the unit whose turn is being prepared.
 *
 * A mounted rider uses its mount's movement allowance.
 */
void ai_init_acting_unit_data(void) {
    ai_data_t* ai;
    unit_stats_t* unit;
    unit_stats_t* acting;
    s32 mount_info;
    s32 id;

    ai = &g_ai_data_base;
    id = g_ai_data_base.acting_unit_id;
    unit = &g_battle_unit_stats[id];
    g_ai_data_base.acting_unit = unit;
    g_ai_data_base.acting_unit_decision = &ai->unit_decisions[id];
    g_ai_data_base.acting_unit_coords.fields.x = unit->x;
    g_ai_data_base.acting_unit_coords.fields.y = unit->position.y;
    g_ai_data_base.acting_unit_coords.fields.elevation = unit->position.higher_elevation;
    g_ai_data_base.acting_unit_coords.fields.zero = 0;
    g_ai_data_base.acting_unit_compact_index = ai->unit_compact_indices[id];
    g_ai_data_base.acting_unit_team = unit->team_flags & BATTLE_TEAM_MASK;
    g_ai_data_base.acting_unit_remaining_clockticks = ai_calculate_clockticks_until_unit_acts(unit);
    acting = g_ai_data_base.acting_unit;
    mount_info = acting->mount_info;
    if (mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        g_ai_data_base.acting_unit_move = g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK].move;
    } else {
        g_ai_data_base.acting_unit_move = acting->move;
    }
    ai->current_action.coords.fields.zero = 0;
    ai->current_action.command.actor_id = ai->acting_unit_id;
}

/*
 * Refresh cached unit decision flags before AI simulation.
 *
 * Keep the simulation flag set across suspension; clear it on completion.
 */
s32 ai_refresh_unit_decision_flags(void) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit;
    ai_unit_decision_t* decision;
    s32 unit_id;

    /* A resumed decision goes straight back to the suspended simulation. */
    if (!g_ai_data_base.resume_pending) {
        for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
            unit = &g_battle_unit_stats[unit_id];
            decision = &ai->unit_decisions[unit_id];
            if (ai_is_unit_untargetable(unit_id)) {
                /* Other cached fields are intentionally untouched for excluded units. */
                decision->targeting_flags_2 |= AI_TARGET_UNTARGETABLE;
                continue;
            }
            if (unit_id != ai->acting_unit_id && (unit->initial_team_flags & BATTLE_TEAM_MASK) != ai->acting_unit_team)
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
    if (ai_simulate_action_and_score() == -1) {
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
void ai_update_team_targeting_flags(void) {
    ai_data_t* ai = &g_ai_data_base;
    ai_unit_decision_t* acting_decision = g_ai_data_base.acting_unit_decision;
    s32 ally_count;
    s32 enemy_count;
    s32 impaired_ally_count;
    s32 i;

    g_ai_data_base.single_enemy_remaining = 0;
    if ((acting_decision->targeting_flags_2 & AI_TARGET_TEAM_SUPPORT_UNAVAILABLE)
        || (g_ai_data_base.acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
            & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE))) {
        g_ai_data_base.targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_RETREAT_SUPPORT_AVAILABLE;
        return;
    }
    ally_count = 0;
    enemy_count = 0;
    impaired_ally_count = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (ai_check_target_type(i)) {
            continue;
        }
        if (ai->unit_decisions[i].enemy_flag) {
            enemy_count++;
        } else {
            ally_count++;
            if (g_battle_unit_stats[i].status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
                & (STATUS_BYTE_MASK(STATUS_ID_FROG) | STATUS_BYTE_MASK(STATUS_ID_CRITICAL))) {
                impaired_ally_count++;
            }
        }
    }
    if (ally_count == impaired_ally_count || ally_count < 2) {
        if (ai->acting_unit_team) {
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
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
void ai_init_target_consideration(void) {
    ai_unit_decision_t* decision;
    ai_data_t* ai;
    unit_stats_t* acting;
    s32 flags;
    u8 targeting_flags;
    ai_nearest_target_mode_e mode;
    s32 target;

    ai = &g_ai_data_base;
    decision = g_ai_data_base.acting_unit_decision;
    if (g_ai_data_base.acting_unit_team == 0) {
        acting = g_ai_data_base.acting_unit;
        if (acting->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
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
                ai_copy_unit_coordinates(decision->main_target_id, &decision->target);
                target = ai_check_target_type(decision->main_target_id);
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
        if ((ai->acting_unit->hp << 7) / ai->acting_unit->max_hp < 45) {
            mode = AI_NEAREST_HEALER_OR_CRYSTAL;
        } else {
            for (target = 0; target < BATTLE_UNIT_SLOT_COUNT; target++)
                ai->targetability.live.unit_targetable[target] = 1;
            mode = AI_NEAREST_ELIGIBLE_ENEMY;
        }
        target = ai_find_nearest_target(mode);
        ai_focus_on_target(target);
    } else {
        targeting_flags = decision->targeting_flags_1;
        if (!(targeting_flags & AI_TARGETING_FLAG_1_MOSTLY_SUPPORT)
            || (decision->targeting_flags_2 & AI_TARGET_TEAM_SUPPORT_UNAVAILABLE)) {
            ai->autobattle_setting = AI_AUTOBATTLE_MANUAL;
            for (target = 0; target < BATTLE_UNIT_SLOT_COUNT; target++)
                ai->targetability.live.unit_targetable[target] = 1;
            mode = AI_NEAREST_ELIGIBLE_ENEMY;
            target = ai_find_nearest_target(mode);
            ai_focus_on_target(target);
        } else {
            ai->autobattle_setting = AI_AUTOBATTLE_FADING_LIFE;
        }
    }
}

/*
 * Focus the acting unit's decision on one target and its coordinates.
 *
 * Set focus flag 0x40 and clear stay-near flag 0x20, retaining other flags.
 */
void ai_focus_on_target(s32 unit_id) {
    ai_unit_decision_t* decision = g_ai_data_base.acting_unit_decision;

    decision->main_target_id = unit_id;
    decision->flags = (decision->flags | AI_DECISION_FOCUS_TARGET) & ~AI_DECISION_STAY_NEAR_COORDINATES;
    ai_copy_unit_coordinates(unit_id, &decision->target);
}

/* Build the AI walkable and reachable tile rows for the acting unit.
 *
 * A fresh decision (resume_pending 0) sets the water penalty, rebuilds the
 * walkable rows, clears the scenario rows and spreads movement unless the
 * unit has moved or cannot move. The reachable rows then drop occupied tiles
 * and, unless the unit is disabled, the four listed coordinates. Returns -1
 * when the frame budget is spent. i and j are reused as the spread unit id
 * and the teleport-range distances, as the target's register assignment
 * shows. */
s32 ai_set_movement_panel_data(s32 movement_taken) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit;
    map_tile_t* tile;
    s32 level;
    s32 y;
    s32 x;
    s32 i;
    u8 mount_info;
    s32 j;
    ai_coords_t* coords;

    if (g_ai_data_base.resume_pending == 0) {
        if ((g_ai_data_base.acting_unit->ability_flags.movement[2]
                & (BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER | BATTLE_MOVEMENT_SET_3_WALK_ON_WATER
                    | BATTLE_MOVEMENT_SET_3_FLOAT))
            || (g_ai_data_base.acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
                & STATUS_BYTE_MASK(STATUS_ID_FLOAT))) {
            g_ai_data_base.water_penalty = 0;
        } else {
            g_ai_data_base.water_penalty = 1;
        }
        mem_zero_32bit_values(ai->walkable_tiles, sizeof(ai->walkable_tiles));
        for (level = 0; level < MAP_TERRAIN_LAYER_COUNT; level++) {
            for (y = 0; y < ai->map_max_y; y++) {
                for (x = 0; x < ai->map_max_x; x++) {
                    tile = &g_map_tile_data[level * MAP_TILES_PER_LAYER + y * ai->map_max_x + x];
                    if (!(tile->flags.blocked) && (tile->surface.type) != MAP_SURFACE_CROSS_SECTION) {
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
            ai->reachable_tiles[0][ai->acting_unit_coords.fields.elevation][ai->acting_unit_coords.fields.y]
                = 0x8000 >> ai->acting_unit_coords.fields.x;
            return 0;
        }
        mount_info = ai->acting_unit->mount_info;
        if (mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
            i = mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK;
        } else {
            i = ai->acting_unit_id;
        }
        move_set_reachable_tiles(i, ai->acting_unit_coords.fields.x, ai->acting_unit_coords.fields.y,
            ai->acting_unit_coords.fields.elevation);
    }
    if (VSync(1) >= AI_DECISION_SCANLINE_LIMIT) {
        return -1;
    }
    ai->resume_pending = 0;
    for (level = 0; level < MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            for (x = 0; x < ai->map_max_x; x++) {
                tile = &g_map_tile_data[level * MAP_TILES_PER_LAYER + y * ai->map_max_x + x];
                if (ai->acting_unit->ability_flags.movement[1] & BATTLE_MOVEMENT_SET_2_TELEPORT) {
                    if (!(ai->acting_unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER)) {
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
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && i != ai->acting_unit_id
            && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
            && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
            ai->reachable_tiles[0][unit->position.higher_elevation][unit->position.y] &= ~(0x8000 >> unit->x);
        }
    }
    ai->reachable_tiles[0][ai->acting_unit_coords.fields.elevation][ai->acting_unit_coords.fields.y]
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
        ai->reachable_tiles[0][coords->fields.elevation][coords->fields.y] &= ~(0x8000 >> coords->fields.x);
    }
    ai->reachable_tiles[0][ai->acting_unit_coords.fields.elevation][ai->acting_unit_coords.fields.y]
        |= 0x8000 >> ai->acting_unit_coords.fields.x;
    return 0;
}

/* Mark each unit's usable abilities and derive the acting unit's AI hints.
 *
 * For every AI-considered unit, abilities blocked by Don't Act, Silence,
 * Innocent, MP, item stock or level are left unmarked; usable ones get entry
 * bit 0x8000 and feed the enemy-team counters and the per-unit helpful ratio.
 * The acting unit's list then drops buff-only abilities the enemies cannot
 * threaten, aggregates cancellable statuses, range limits and the random-use
 * threshold. action_taken clears the usable bits again.
 *
 * threshold holds first the status-check CT and later the enemy-count cutoff;
 * the single variable reproduces the target's register allocation. */
void ai_set_ability_considerations(s32 action_taken) {
    ai_data_t* ai;
    ai_considered_ability_t* ability;
    s32 delay;
    s32 enemy_harmful;
    s32 enemy_magical;
    s32 enemy_physical;
    s32 enemy_reflectable;
    s32 enemy_earth;
    s32 unit_harmful;
    s32 unit_helpful;
    s32 lowest_ratio;
    s32 lowest_unit;
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

    enemy_harmful = 0;
    enemy_magical = 0;
    enemy_physical = 0;
    enemy_reflectable = 0;
    enemy_earth = 0;
    lowest_ratio = 0xff;
    lowest_unit = 0xff;
    ai = &g_ai_data_base;
    ability = &g_ai_data_base.considered_ability;
    unit_id = 0;
    do {
        if (ai_check_target_type(unit_id) == 0) {
            decision = &ai->unit_decisions[unit_id];
            unit = &g_battle_unit_stats[unit_id];
            unit_harmful = 0;
            unit_helpful = 0;
            if (unit_id == g_ai_data_base.acting_unit_id) {
                delay = 0;
            } else {
                delay = ai_calculate_clockticks_until_unit_acts(unit);
            }
            decision->targeting_flags_1
                &= ~(AI_TARGETING_FLAG_1_HAS_RANGED_ABILITY | AI_TARGETING_FLAG_1_CAN_HEAL_ALLIES);
            for (i = 0; i < AI_ABILITY_LIST_COUNT; i++) {
                entry = &ai->ability_lists[ai->unit_compact_indices[unit_id]][i];
                entry->usage.policy.usable = 0;
                if (entry->skillset == SKILLSET_ID_NONE) {
                    break;
                }
                ai_load_ability_entry(entry);
                ability_id = ability->ability_id;
                threshold = delay + ability->ct;
                if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
                    if (ability_id != ABILITY_ID_BLACK_MAGIC_FROG && ability_id != ABILITY_ID_FROG_ATTACK) {
                        continue;
                    }
                } else if (ability_id == ABILITY_ID_FROG_ATTACK) {
                    continue;
                }
                if (ai_is_status_active_through_delay(delay, unit, STATUS_ID_DONT_ACT)) {
                    continue;
                }
                if ((ability->ai_flags.groups.target_effect_flags & AI_ABILITY_AFFECTED_BY_SILENCE)
                    && ai_is_status_active_through_delay(threshold, unit, STATUS_ID_SILENCE)) {
                    continue;
                }
                if ((ability->ai_flags.groups.target_effect_flags & AI_ABILITY_AFFECTED_BY_FAITH)
                    && ai_is_status_active_through_delay(threshold, unit, STATUS_ID_INNOCENT)) {
                    continue;
                }
                if (ability->mp_cost > unit->mp) {
                    continue;
                }
                menu = g_main_action_menu_types_by_skillset[entry->skillset];
                if (menu == ACTION_MENU_TYPE_ITEM_INVENTORY || menu == ACTION_MENU_TYPE_KATANA_INVENTORY
                    || ability_id == ABILITY_ID_THROW_SHURIKEN || ability_id == ABILITY_ID_THROW_BALL) {
                    if (!(unit->initial_team_flags & BATTLE_TEAM_MASK)) {
                        if (g_main_item_quantities[ability->parameters.fields.item_id] == 0) {
                            continue;
                        }
                    } else if (g_main_item_primary_data[ability->parameters.fields.item_id].required_level
                        > unit->level) {
                        continue;
                    }
                } else if (menu == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
                    if (unit_id != g_ai_data_base.acting_unit_id) {
                        continue;
                    }
                    best_level = -1;
                    for (throw_id = ABILITY_ID_THROW_KNIFE, type_index = 1; throw_id < ABILITY_ID_THROW_BALL;
                        throw_id++, type_index++) {
                        if (ai_is_ability_known(unit_id, ability->skillset, throw_id - (ABILITY_ID_THROW_KNIFE - 2))) {
                            item_type = g_main_throw_ability_item_types[type_index];
                            for (item_id = 0; item_id < ITEM_ID_COUNT; item_id++) {
                                if (g_main_item_primary_data[item_id].type == item_type) {
                                    level = g_main_item_primary_data[item_id].required_level;
                                    if (level >= best_level) {
                                        if (!(unit->initial_team_flags & BATTLE_TEAM_MASK)) {
                                            lacking = g_main_item_quantities[item_id] < 5;
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
                            if (ability->element & BATTLE_ELEMENT_EARTH) {
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
    } while (unit_id < BATTLE_UNIT_SLOT_COUNT);

    usable = 0;
    if (lowest_unit != 0xff) {
        ai->unit_decisions[lowest_unit].targeting_flags_1 &= ~AI_TARGETING_FLAG_1_MOSTLY_SUPPORT;
    }
    random_use = 0;
    decision = g_ai_data_base.acting_unit_decision;
    ai->has_mp_recovery_ability = 0;
    ai->targetability.live.target_setting_flags &= ~AI_TARGET_SETTING_HAS_HOLY_WATER;
    decision->ally_ability_min_reach = 0xff;
    decision->enemy_ability_min_reach = 0xff;
    threshold = enemy_harmful / 8;
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        ai->status_to_cancel[i] = 0;
    }
    for (i = 0; i < AI_ABILITY_LIST_COUNT; i++) {
        entry = &ai->ability_lists[g_ai_data_base.acting_unit_compact_index][i];
        if (entry->skillset == SKILLSET_ID_NONE) {
            break;
        }
        ai_load_ability_entry(entry);
        if (entry->usage.policy.usable) {
            if (threshold >= enemy_magical) {
                ai_disable_ability_if_only_inflicts_status(entry, STATUS_ID_SHELL);
            }
            if (threshold >= enemy_physical) {
                ai_disable_ability_if_only_inflicts_status(entry, STATUS_ID_PROTECT);
            }
            if (threshold >= enemy_reflectable) {
                ai_disable_ability_if_only_inflicts_status(entry, STATUS_ID_REFLECT);
            }
            if (enemy_earth <= 0) {
                ai_disable_ability_if_only_inflicts_status(entry, STATUS_ID_FLOAT);
            }
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
                    if (g_main_action_menu_types_by_skillset[ability->skillset] == ACTION_MENU_TYPE_ITEM_INVENTORY) {
                        range = (range + 1) / 2;
                    }
                    if (range < decision->ally_ability_min_reach) {
                        decision->ally_ability_min_reach = range;
                    }
                }
                if (entry->usage.policy.force_consume_evaluated_origins) {
                    entry->usage.policy.consume_evaluated_origins = 1;
                } else if (!entry->usage.policy.exhaustive_origin_scan) {
                    if (ability->ct >= g_ai_data_base.acting_unit_remaining_clockticks
                        || ai_classify_ability_effect(g_ai_data_base.acting_unit_id, 1) == AI_ABILITY_EFFECT_NONE) {
                        entry->usage.policy.consume_evaluated_origins = 1;
                    } else {
                        entry->usage.policy.consume_evaluated_origins = 0;
                    }
                }
                if (action_taken != 0) {
                    entry->usage.policy.usable = 0;
                }
            }
        }
    }
    if (decision->enemy_ability_min_reach != 0xff) {
        decision->enemy_ability_min_reach += g_ai_data_base.acting_unit_move;
    }
    if (decision->ally_ability_min_reach == 0xff) {
        decision->ally_ability_min_reach = 0;
    }
    decision->ally_ability_min_reach += g_ai_data_base.acting_unit_move;
    if (usable != 0) {
        ai->random_use_threshold = random_use * AI_ABILITY_SHARE_SCALE / usable;
    }
}

/*
 * Disable an AI ability that does nothing except add one unwanted status.
 *
 * The 16-bit value update spans skillset and usage_flags so GCC emits the target's
 * single lhu/andi/sh sequence while clearing only the usable bit. A direct
 * status_infliction pointer folds +7 into its base; byte-stepping the typed AI
 * container retains the target's base pointer and named field access.
 */
void ai_disable_ability_if_only_inflicts_status(ai_ability_entry_t* entry, s32 status_id) {
    ai_data_t* ai = &g_ai_data_base;

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

    {
        s32 status_set;
        s32 mask;
        ai_data_t* cursor;
        s32 shift;

        status_set = 0;
        mask = 0x80;
        cursor = &g_ai_data_base;
        shift = status_id;
        for (; status_set < STATUS_BYTE_COUNT; status_set++, shift -= 8) {
            if (status_set == status_id / 8) {
                if ((cursor->considered_ability.parameters.fields.status_infliction[0] & ((mask >> shift) ^ 0xFF))
                    != 0) {
                    return;
                }
                /* A separate increment per branch keeps the target's cursor
                 * base; one shared increment folds the field offset into it. */
                cursor = (ai_data_t*)((u8*)cursor + 1);
                continue;
            }
            if (cursor->considered_ability.parameters.fields.status_infliction[0] != 0) {
                return;
            }
            cursor = (ai_data_t*)((u8*)cursor + 1);
        }
    }
    entry->usage.policy.usable = 0;
}

/*
 * Load an ability-list entry into the considered AI ability.
 *
 * Decode its physical unit and specialize inventory, weapon and timing data.
 */
void ai_load_ability_entry(const ai_ability_entry_t* entry) {
    ai_data_t* ai = &g_ai_data_base;
    s32 ability_id, unit_id, menu, status_id;
    unit_stats_t* unit;
    ai_weapon_data_t* weapon;
    ability_secondary_data_t* secondary;
    s32 value;

    mem_zero_32bit_values(&ai->considered_ability, sizeof(ai_considered_ability_t));
    mem_zero_32bit_values(&ai->current_action.command.calculator_type, 4);
    ai->considered_ability.skillset = entry->skillset;
    ability_id = g_ai_data_base.considered_ability.ability_id = entry->packed_id & AI_ABILITY_ENTRY_ABILITY_MASK;
    unit_id = entry->packed_id >> AI_ABILITY_ENTRY_UNIT_SHIFT;
    unit = &g_battle_unit_stats[unit_id];
    menu = g_main_action_menu_types_by_skillset[entry->skillset];
    status_id = 0;
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
        status_id = g_main_item_secondary_data[ability_id - ABILITY_ID_ITEM_FIRST].status_infliction_id;
        if (unit->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_THROW_ITEM)
            ai->considered_ability.parameters.fields.range = unit->move;
        else
            ai->considered_ability.parameters.fields.range = 1;
        ai->considered_ability.parameters.fields.aoe = 0;
        ai->considered_ability.parameters.fields.item_id
            = g_main_item_ability_item_ids[ability_id - ABILITY_ID_ITEM_FIRST];
        break;
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        ai->considered_ability.parameters.fields.item_id
            = ability_id - (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
        break;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        if (ability_id >= ITEM_ID_SHURIKEN && ability_id <= ITEM_ID_YAGYU_DARKNESS) {
            ai->considered_ability.ability_id = ABILITY_ID_THROW_SHURIKEN;
            ai->considered_ability.parameters.fields.item_id = ability_id;
            ai->considered_ability.element = g_main_item_weapon_data[ability_id].element;
        } else if (ability_id >= ITEM_ID_FIRE_BALL && ability_id <= ITEM_ID_LIGHTNING_BALL) {
            ai->considered_ability.ability_id = ABILITY_ID_THROW_BALL;
            ai->considered_ability.parameters.fields.item_id = ability_id;
            ai->considered_ability.element = g_main_item_weapon_data[ability_id].element;
        } else {
            ai->considered_ability.ability_id = ai->throw_ability_id;
            ai->considered_ability.parameters.fields.item_id = ai->throw_weapon_id;
        }
        ai->considered_ability.parameters.fields.range = unit->move;
        ability_id = ai->considered_ability.ability_id;
        break;
    case ACTION_MENU_TYPE_JUMP:
        if (unit_id == ai->acting_unit_id)
            ai->considered_ability.ct
                = ((UNIT_TURN_CT_THRESHOLD - ai->acting_unit->ct) / ai->acting_unit->attributes[UNIT_ATTRIBUTE_SPEED])
                / 2;
        else {
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
                = g_main_jump_ability_data[ability_id - ABILITY_ID_JUMP_FIRST].range;
        weapon = &ai->unit_weapon_data[ai->unit_compact_indices[unit_id]];
        goto weapon_flags;
    case ACTION_MENU_TYPE_CHARGE:
        ai->considered_ability.ct = g_main_charge_ability_data[ability_id - ABILITY_ID_CHARGE_FIRST].ct;
        goto attack;
    case ACTION_MENU_TYPE_ARITHMETICKS: {
        u32 status_cursor;
        s32 included = 0xff;
        value = 4;
        /* A direct status pointer folds the member offset into its base;
         * retain the field-derived base cursor for the exact five-byte fill. */
        status_cursor = (u32)ai + value;
        do {
            *(u8*)(status_cursor + ((u32)&ai->considered_ability.parameters.fields.status_infliction - (u32)ai))
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
    mem_copy_u16_values(&ai->considered_ability.ai_flags, &g_main_ability_data[ability_id].ai_flags, 4);
    if (menu == ACTION_MENU_TYPE_ITEM_INVENTORY || menu == ACTION_MENU_TYPE_WEAPON_INVENTORY)
        ai->considered_ability.ai_flags.word |= AI_ABILITY_DIRECT_TRAJECTORY;
    ai->considered_ability.ai_flags.fields.flags_4 = entry->usage.flags;
    if (ability_id < ABILITY_ID_ITEM_FIRST) {
        s32 ct;
        secondary = &g_main_ability_range_data[ability_id];
        ct = secondary->ct;
        status_id = secondary->status_infliction_id;
        ai->considered_ability.ct = ct;
        ai->considered_ability.element = secondary->element;
        ai->considered_ability.mp_cost = secondary->mp_cost;
        if (unit->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_HALF_MP)
            ai->considered_ability.mp_cost >>= 1;
        if (secondary->flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE) {
            ai->considered_ability.parameters.fields.range
                = ai->unit_weapon_data[ai->unit_compact_indices[unit_id]].fields.range;
            ai->considered_ability.parameters.fields.aoe = 0;
        } else {
            ai->considered_ability.parameters.fields.range = secondary->range;
            ai->considered_ability.parameters.fields.aoe = secondary->aoe;
        }
        if (secondary->flags_3 & ABILITY_SECONDARY_FLAG_3_PERSEVERE)
            goto copy_status;
    }
    /* Preserve the native support-byte masks, including their CT quirk. */
    if (unit->ability_flags.support[3] & BATTLE_SUPPORT_SET_4_MONSTER_SKILL)
        ai->considered_ability.ct >>= 1;
    else if (unit->ability_flags.support[3] & BATTLE_SUPPORT_SET_4_DEFEND)
        ai->considered_ability.ct = 0;
copy_status:
    mem_copy_bytes(ai->considered_ability.parameters.fields.status_infliction,
        g_main_status_infliction_data[status_id].statuses,
        sizeof(ai->considered_ability.parameters.fields.status_infliction));
}

/*
 * Initialize and dispatch the AI action-selection passes.
 *
 * Resume the selected pass without repeating action-record initialization.
 * Return -1 to suspend and 0 when that pass completes.
 */
s32 ai_select_action_initial(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 result;

    if (g_ai_data_base.resume_pending) {
        /* Resume: re-enter the step that suspended. */
        switch (g_ai_data_base.initial_selection_stage) {
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
        ai_reset_action_ranking_fields(&ai->ranked_actions[result]);
    ai_reset_action_ranking_fields(&ai->current_action);
    ai_reset_action_ranking_fields(&ai->best_action);
    ai_reset_action_ranking_fields(&ai->inverted_priority_action);
    ai_reset_action_ranking_fields(&ai->selected_action);
    ai_fill_foe_proximity_grid();
status_action:
    result = ai_select_action_status();
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
        mem_copy_u16_values(&ai->saved_acting_unit_decision, ai->acting_unit_decision, sizeof(ai_unit_decision_t));
    /* Resume after the save; restore the decision only when this pass ends. */
    retreat_action:
        if (ai_select_action_retreat() == -1) {
            ai->initial_selection_stage = AI_INITIAL_SELECTION_RETREAT;
            return -1;
        }
        mem_copy_u16_values(ai->acting_unit_decision, &ai->saved_acting_unit_decision, sizeof(ai_unit_decision_t));
        return 0;
    }
autobattle_action:
    result = ai_select_action_autobattle();
    if (result == -1) {
        ai->initial_selection_stage = AI_INITIAL_SELECTION_AUTOBATTLE;
        return -1;
    }
    if (result != 0)
        return 0;
    ai->resume_pending = 0;
general_action:
    if (ai_select_action_general() == -1) {
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
void ai_reset_action_ranking_fields(ai_action_data_t* action) {
    /* Signed view preserves addiu -32767; direct u16 assignment emits ori. */
    action->rank.priority = -0x7fff;
    action->rank.decided = 0;
    action->rank.base_hit_percent = 0;
    action->wait_facing_hint = AI_DIRECTION_CHOOSE_AT_WAIT;
    action->reflected_action = 0;
}

/* Evaluate movement while keeping or removing the acting unit's charge.
 *
 * The second simulation temporarily changes unit data; a suspended pass
 * preserves those changes until resumed and completed. The simulation helper
 * resumes from its saved phase when the frame budget is exhausted.
 */
s32 ai_evaluate_charging_movement(void) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit = g_ai_data_base.acting_unit;
    action_command_t* action = &unit->action_command;
    ai_charging_stage_e state;

    if (g_ai_data_base.resume_pending) {
        /* Resume: re-enter the step that suspended. */
        state = g_ai_data_base.charging_state;
        if (state == AI_CHARGING_SONG_DANCE)
            goto evaluate_song_dance;
        if (state == AI_CHARGING_WITH_CHARGE)
            goto evaluate_with_charge;
        goto evaluate_without_charge;
    }
    if (unit->charged_ability_ct == 0xff)
        return 0;
    if (g_main_action_menu_types_by_skillset[unit->action_command.skillset_id] == ACTION_MENU_TYPE_CHARGE) {
        g_ai_data_base.current_action.coords = g_ai_data_base.acting_unit_coords;
    } else {
        g_ai_data_base.current_action.coords = ai->candidate_coords[g_ai_data_base.movement_scenario];
        ai_update_tile_pickup();
    }
    ai->current_action.act_before_move = 0;
    ai->current_action.command.skillset_id = 0;
    if (action->ability_id >= ABILITY_ID_SONG_ANGEL_SONG && action->ability_id <= ABILITY_ID_DANCE_LAST_DANCE) {
    evaluate_song_dance:
        if (ai_evaluate_current_action() == -1) {
            ai->charging_state = AI_CHARGING_SONG_DANCE;
            return -1;
        }
        ai_insert_ranked_action();
        return 0;
    }
evaluate_with_charge:
    if (ai_simulate_action_and_score() == -1) {
        ai->charging_state = AI_CHARGING_WITH_CHARGE;
        return -1;
    }
    mem_copy_u16_values(&ai->selected_action, &ai->current_action, sizeof(ai_action_data_t));
    ai->saved_ability_ct = ai->acting_unit->charged_ability_ct;
    ai->saved_current_status = ai->acting_unit->status_sets.current[0];
    ai->saved_inflicted_status = ai->acting_unit->inflicted_status[0];
    ai->acting_unit->charged_ability_ct = 0xff;
    ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
        &= ~(STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
    ai->acting_unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
        &= ~(STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
evaluate_without_charge:
    if (ai_simulate_action_and_score() != -1) {
        ai->acting_unit->charged_ability_ct = ai->saved_ability_ct;
        ai->acting_unit->status_sets.current[0] = ai->saved_current_status;
        ai->acting_unit->inflicted_status[0] = ai->saved_inflicted_status;
        return ai_compare_target_priority_and_hit_rate(&ai->selected_action.rank, &ai->current_action.rank)
            == AI_RANK_ABOVE;
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
s32 ai_select_action_general(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 charging_result;
    ai_autobattle_stage_e resume_state;

    if (g_ai_data_base.resume_pending != 0) {
        /* Resume: re-enter the step that suspended. */
        resume_state = g_ai_data_base.autobattle_state;
        if (resume_state == AI_AUTOBATTLE_MOVEMENT)
            goto evaluate_movement;
        if (resume_state == AI_AUTOBATTLE_CHARGING)
            goto evaluate_charging;
        goto evaluate_abilities;
    }
    g_ai_data_base.movement_scenario = 1;
evaluate_movement:
    if (ai_build_movement_scenarios() == -1) {
        ai->autobattle_state = AI_AUTOBATTLE_MOVEMENT;
        return -1;
    }
    ai->movement_scenario = 0;
    ai_invert_target_priority();
    ai->movement_scenario = 1;
evaluate_charging:
    charging_result = ai_evaluate_charging_movement();
    switch (charging_result) {
    case -1:
        ai->autobattle_state = AI_AUTOBATTLE_CHARGING;
        return -1;
    case 1:
        return 0;
    }
    ai->movement_scenario = 1;
    if (ai_select_active_units(AI_UNIT_FILTER_ANY, 0) != 0) {
    evaluate_abilities:
        if (ai_evaluate_ability_list() == -1) {
            ai->autobattle_state = AI_AUTOBATTLE_ABILITIES;
            return -1;
        }
    }
    if (ai_select_action_ranked() == 0) {
        ai->movement_scenario = 0;
        ai_select_action_fallback();
    }
    return 0;
}

/*
 * Choose the range threshold used to partition movement candidates.
 *
 * A focused enemy uses enemy_ability_min_reach; a focused ally uses ally_ability_min_reach.
 * Without a focused target, use Move. The caller consumes the full register.
 */
s32 ai_get_movement_range(void) {
    ai_data_t* ai = &g_ai_data_base;
    ai_unit_decision_t* acting = g_ai_data_base.acting_unit_decision;
    if (acting->flags & AI_DECISION_FOCUS_TARGET) {
        if (ai->unit_decisions[acting->main_target_id].enemy_flag != 0) {
            return acting->enemy_ability_min_reach;
        }
        return acting->ally_ability_min_reach;
    }
    return g_ai_data_base.acting_unit_move;
}

enum {
    AI_BASE_HIT_PERCENT = 100,
    AI_TILE_ROW_BYTE_STRIDE = AI_GRID_COLUMN_COUNT * sizeof(s16),
    AI_TILE_LEVEL_BYTE_STRIDE = AI_GRID_ROW_COUNT * AI_TILE_ROW_BYTE_STRIDE,
};

/* 0x5f4; kept as a separate integer term so the target's addu/addiu order
 * survives (a typed &ai->tile_priority[...] folds the offset first). */
#define AI_TILE_PRIORITY_OFFSET ((u32) & ((ai_data_t*)0)->tile_priority)

/*
 * Copies the candidate tile's priority into the inverted-priority scratch
 * action with its sign flipped.  tile_priority is a
 * grid of 16 columns x 18 signed 16-bit values per map level. The lookup reads
 * the stored 16-bit pattern before negating them.
 */
void ai_invert_target_priority(void) {
    ai_data_t* ai;
    ai_coords_t* coords;
    u8* level_row;
    u8 scenario;
    u32 row;

    ai = &g_ai_data_base;
    scenario = g_ai_data_base.movement_scenario;
    g_ai_data_base.inverted_priority_action.rank.decided = 0;
    g_ai_data_base.inverted_priority_action.rank.base_hit_percent = AI_BASE_HIT_PERCENT;
    coords = &ai->candidate_coords[scenario];
    level_row = (u8*)ai + coords->fields.elevation * AI_TILE_LEVEL_BYTE_STRIDE + AI_TILE_PRIORITY_OFFSET;
    row = coords->fields.y * AI_TILE_ROW_BYTE_STRIDE + (u32)level_row;
    g_ai_data_base.inverted_priority_action.rank.priority = -*(u16*)(coords->fields.x * 2 + row);
}

/* Workspace aliases preserve the target's separate entry address loads. */
extern s32 g_ai_target_in_range;             /* 1 for range >= distance or charging; 0 otherwise. */
extern s32 g_ai_closest_ability_range;       /* Movement range, possibly halved before tile selection. */
extern s32 g_ai_movement_unit_index;         /* Reused unit/row loop index or Stay Near occupant ID. */
extern s32 g_ai_target_distance;             /* Target-distance byte at the acting unit's tile. */
extern s32 g_ai_stay_near_destination_fixed; /* 1 after a reachable Stay Near destination is fixed. */

/* Prepare candidate destinations and reachable movement scenarios.
 *
 * Return -1 when a child suspends and 0 after all scenarios are prepared.
 * 32-bit scratch values preserve branch decisions across suspension.
 */
s32 ai_build_movement_scenarios(void) {
    ai_unit_decision_t* decision;
    ai_data_t* ai;
    unit_stats_t* unit;
    s32 limit;
    s32 low;
    s32 high;

    ai = &g_ai_data_base;
    g_ai_data_base.movement_scenario = 1;
    decision = g_ai_data_base.acting_unit_decision;
    if (g_ai_data_base.resume_pending) {
        /* Resume: re-enter the step that suspended. */
        if (g_ai_data_base.movement_decision_state == AI_MOVEMENT_TARGET_DISTANCE)
            goto fill_target_distance;
        goto score_tiles;
    }
    if ((decision->flags & AI_DECISION_FOCUS_TARGET)
        && (decision->main_target_id == g_ai_data_base.acting_unit_id
            || ((ai->unit_decisions[decision->main_target_id].targeting_flags_2 >> 4)
                & (AI_TARGET_UNTARGETABLE >> 4)))) {
        mem_zero_32bit_values(ai->tile_target_distance, sizeof(ai->tile_target_distance));
    } else {
    fill_target_distance:
        if (ai_fill_target_distance_grid(&decision->target) == -1) {
            ai->movement_decision_state = AI_MOVEMENT_TARGET_DISTANCE;
            return -1;
        }
        if ((decision->targeting_flags_2 & (AI_TARGET_HP_BELOW_HALF | AI_TARGET_MP_CONSTRAINED))
            && !(ai->acting_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
                & STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE))) {
            for (g_ai_movement_unit_index = 0; g_ai_movement_unit_index < BATTLE_UNIT_SLOT_COUNT;
                g_ai_movement_unit_index++) {
                unit = &g_battle_unit_stats[g_ai_movement_unit_index];
                if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE
                    && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                        & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))) {
                    ai_copy_unit_coordinates(g_ai_movement_unit_index, &ai->considered_unit_coords);
                    ai->tile_target_distance[ai->considered_unit_coords.fields.elevation]
                                            [ai->considered_unit_coords.fields.y][ai->considered_unit_coords.fields.x]
                        = 1;
                }
            }
        }
    }
    g_ai_target_distance = ai->tile_target_distance[ai->acting_unit_coords.fields.elevation]
                                                   [ai->acting_unit_coords.fields.y][ai->acting_unit_coords.fields.x];
    g_ai_closest_ability_range = ai_get_movement_range();
    limit = g_ai_target_distance;
    g_ai_stay_near_destination_fixed = 0;
    if (g_ai_closest_ability_range >= limit || (decision->targeting_flags_1 & AI_TARGETING_FLAG_1_REFLECTED_ACTION)) {
        g_ai_target_in_range = 1;
        if ((decision->flags & AI_DECISION_STAY_NEAR_COORDINATES)
            && ((g_ai_movement_unit_index = ai_find_unit_at_coordinates(&decision->target)) == ai->acting_unit_id
                || g_ai_movement_unit_index == BATTLE_UNIT_ID_NONE)
            && (ai->reachable_tiles[0][decision->target.fields.elevation][decision->target.fields.y]
                & (0x8000 >> decision->target.fields.x))) {
            ai->candidate_coords[ai->movement_scenario] = decision->target;
            ai_set_candidate_and_actor_reachable_tiles();
            g_ai_stay_near_destination_fixed = 1;
        } else {
            ai_filter_reachable_tiles_by_target_distance(ai->acting_unit_move + g_ai_target_distance);
        }
    } else {
        g_ai_target_in_range = 0;
        ai_filter_reachable_tiles_by_target_distance(limit - 1);
    }
score_tiles:
    if (ai_score_reachable_tiles() == -1) {
        ai->movement_decision_state = AI_MOVEMENT_SCORE_TILES;
        return -1;
    }
    if (!g_ai_stay_near_destination_fixed) {
        if (g_ai_target_in_range) {
            ai_select_destination(0x7fffffff, AI_DESTINATION_PRIORITY_FIRST);
        } else {
            if (ai->acting_unit_team && rand() % 7 == 0)
                g_ai_closest_ability_range /= 2;
            ai_select_destination_with_nearest_fallback(g_ai_closest_ability_range);
        }
    }
    /* XOR preserves the target operation without assuming scenario 1 is a subset. */
    ai->movement_scenario = 2;
    for (g_ai_movement_unit_index = 0; g_ai_movement_unit_index < ai->map_max_y; g_ai_movement_unit_index++) {
        low = ai->reachable_tiles[0][0][g_ai_movement_unit_index] ^ ai->reachable_tiles[1][0][g_ai_movement_unit_index];
        high
            = ai->reachable_tiles[0][1][g_ai_movement_unit_index] ^ ai->reachable_tiles[1][1][g_ai_movement_unit_index];
        ai->reachable_tiles[2][0][g_ai_movement_unit_index] = low;
        ai->reachable_tiles[2][1][g_ai_movement_unit_index] = high;
    }
    ai->candidate_coords[2] = ai->candidate_coords[1];
    ai->movement_scenario = 0;
    if (!ai->acting_unit->action_taken && (decision->flags & AI_DECISION_FOCUS_TARGET) && g_ai_target_in_range
        && !(decision->targeting_flags_1 & AI_TARGETING_FLAG_1_REFLECTED_ACTION)) {
        ai_select_destination_with_range_floor(g_ai_target_distance - 1);
    } else {
        ai->candidate_coords[0] = ai->candidate_coords[1];
    }
    ai->reachable_tiles[1][ai->acting_unit_coords.fields.elevation][ai->acting_unit_coords.fields.y]
        |= 0x8000 >> ai->acting_unit_coords.fields.x;
    return 0;
}

/*
 * Select a destination with the conditional three-tile distance floor.
 *
 * Against an enemy target, targeting flag 0x01 raises smaller limits to 3.
 * Selection still uses the ordinary unrestricted distance-first fallback.
 */
void ai_select_destination_with_range_floor(s32 distance_limit) {
    ai_unit_decision_t* decision = g_ai_data_base.acting_unit_decision;
    if (g_ai_data_base.unit_decisions[decision->main_target_id].enemy_flag != 0
        && (decision->targeting_flags_1 & AI_TARGETING_FLAG_1_HAS_RANGED_ABILITY) && distance_limit < 3) {
        distance_limit = 3;
    }
    ai_select_destination_with_nearest_fallback(distance_limit);
}

/* Prefer priority within range, falling back to the nearest reachable tile.
 *
 * The fallback removes the distance bound. Neither pass requires a positive
 * priority. */
void ai_select_destination_with_nearest_fallback(s32 range) {
    if (ai_select_destination(range, AI_DESTINATION_PRIORITY_FIRST) == 0) {
        ai_select_destination(0x7fffffff, AI_DESTINATION_DISTANCE_FIRST);
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
s32 ai_select_destination(s32 max_distance, ai_destination_order_e order) {
    s32 x, y, level;
    s32 best_score = 0x7fffffff;
    s32 best_distance = 0x7fffffff;
    ai_data_t* ai = &g_ai_data_base;
    ai_coords_t* destination = &ai->candidate_coords[g_ai_data_base.movement_scenario];
    u16 row;
    s32 distance, score;

    for (level = 0; level < MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            {
                u32 scenario_address = ai->movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
                u32 level_address;
                scenario_address += (u32)&ai->reachable_tiles - (u32)ai;
                level_address = level * sizeof(ai->reachable_tiles[0][0]) + scenario_address;
                row = *(u16*)(y * sizeof(ai->reachable_tiles[0][0][0]) + level_address);
            }
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if ((row << x) & 0x8000) {
                        u32 level_tile_index = y << 4;
                        u32 tile_index;
                        u32 byte_address;
                        level_tile_index += x;
                        tile_index = level * sizeof(ai->tile_target_distance[0]) + level_tile_index;
                        byte_address = tile_index + (u32)ai;
                        distance = *(u8*)(byte_address + ((u32)&ai->tile_target_distance - (u32)ai));
                        if (max_distance < distance) {
                            continue;
                        }
                        {
                            s32 proximity = *(u8*)(byte_address + ((u32)&ai->tile_foe_proximity - (u32)ai));
                            score = *(u16*)(tile_index * sizeof(ai->tile_priority[0][0][0]) + (u32)ai
                                + ((u32)&ai->tile_priority - (u32)ai));
                            /* Compare the packed priority and proximity as a signed score. */
                            score = score << 16;
                            score += proximity;
                        }
                        if (order != AI_DESTINATION_PRIORITY_FIRST) {
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
                        destination->fields.elevation = level;
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
s32 ai_filter_reachable_tiles_by_target_distance(s32 distance_limit) {
    s32 found = 0;
    ai_data_t* ai = &g_ai_data_base;
    s32 x, y, level;
    u16 row;

    for (x = 0; x < AI_GRID_ROW_COUNT; x++) {
        ai->reachable_tiles[ai->movement_scenario][0][x] = 0;
        ai->reachable_tiles[ai->movement_scenario][1][x] = 0;
    }
    for (level = 0; level < MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            row = ai->reachable_tiles[0][level][y];
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if (((row << x) & 0x8000) && distance_limit >= ai->tile_target_distance[level][y][x]) {
                        ai->reachable_tiles[ai->movement_scenario][level][y] |= 0x8000 >> x;
                        found = 1;
                    }
                }
            }
        }
    }
    if (found == 0) {
        mem_copy_u16_values(
            ai->reachable_tiles[ai->movement_scenario], ai->reachable_tiles[0], sizeof(ai->reachable_tiles[0]));
        return 0;
    }
    return 1;
}

/* 1 when the unit is untargetable or dead without Reraise per its AI decision
 * record, or petrified. */
s32 ai_check_target_type(s32 unit_id) {
    u8 flags = g_ai_data_base.unit_decisions[unit_id].targeting_flags_2;

    if (((flags >> 4) & (AI_TARGET_UNTARGETABLE >> 4)) || ((flags >> 5) & (AI_TARGET_DEAD_WITHOUT_RERAISE >> 5))) {
        return 1;
    }
    return g_battle_unit_stats[unit_id].status_sets.current[1] >> 7;
}

/* Excludes absent, inert, mounted, or hidden enemies from ordinary AI targeting.
 *
 * The native code deliberately uses only 4 bits for the rider's partner ID,
 * unlike other mount consumers' 5-bit mask. A Transparent rider is admitted
 * only when its mount is not also Transparent. */
s32 ai_is_unit_untargetable(s32 unit_idx) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit;
    s32 mount;

    unit = &g_battle_unit_stats[unit_idx];
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return 1;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        return 1;
    }
    mount = unit->mount_info;
    if (mount & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        return 1;
    }
    if (ai->unit_decisions[unit_idx].enemy_flag == 0) {
        return 0;
    }
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)] & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))
        && (!(mount & BATTLE_MOUNT_INFO_FLAG_RIDER)
            || !(g_battle_unit_stats[mount & 0xf].status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
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
ai_rank_order_e ai_compare_target_priority_and_hit_rate(const ai_action_rank_t* first, const ai_action_rank_t* second) {
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
s32 ai_is_action_higher_ranked(const ai_action_data_t* first, const ai_action_data_t* second) {
    ai_data_t* ai = &g_ai_data_base;
    ai_rank_order_e comparison;
    s32 proximity_a;
    s32 proximity_b;
    s32 value;

    comparison = ai_compare_target_priority_and_hit_rate(&first->rank, &second->rank);
    value = 1;
    if (comparison == AI_RANK_ABOVE)
        return value;
    if (comparison == AI_RANK_EQUAL) {
        value = first->coords.fields.elevation;
        proximity_a = ai->tile_foe_proximity[value][first->coords.fields.y][first->coords.fields.x];
        proximity_b
            = ai->tile_foe_proximity[second->coords.fields.elevation][second->coords.fields.y][second->coords.fields.x];
        return proximity_a < proximity_b;
    }
    return 0;
}

/* Score the considered ability cast from the unit's own tile (act, then move)
 * and, when the action's destination tile differs and allows the ability, cast
 * from there (move, then act). -1 suspends. */
s32 ai_evaluate_cast_origins(void) {
    ai_data_t* ai;

    ai = &g_ai_data_base;
    if (g_ai_data_base.resume_pending != 0) {
        /* Resume: re-enter the step that suspended. */
        if (g_ai_data_base.cast_origin_stage != AI_CAST_ORIGIN_ACT_BEFORE_MOVE) {
            goto score_move_before_act;
        }
        goto score_act_before_move;
    }
    if (ai_is_ability_origin_allowed(&ai->acting_unit_coords) != 0) {
        g_ai_data_base.current_action.act_before_move = 1;
    score_act_before_move:
        if (ai_evaluate_current_action() == -1) {
            ai->cast_origin_stage = AI_CAST_ORIGIN_ACT_BEFORE_MOVE;
            return -1;
        }
    }
    if (ai->current_action.coords.word == ai->acting_unit_coords.word) {
        return 0;
    }
    if (ai_is_ability_origin_allowed(&ai->current_action.coords) == 0) {
        return 0;
    }
    ai->current_action.act_before_move = 0;
score_move_before_act:
    if (ai_evaluate_current_action() != -1) {
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
void ai_insert_ranked_action(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 i;
    s32 j;

    if (ai_compare_target_priority_and_hit_rate(&ai->best_action.rank, &ai->inverted_priority_action.rank)
        == AI_RANK_ABOVE) {
        if (ai_is_action_higher_ranked(&ai->best_action, &ai->ranked_actions[AI_RANKED_ACTION_COUNT - 1])) {
            for (i = 0; i < AI_RANKED_ACTION_COUNT; i++) {
                if (ai_is_action_higher_ranked(&ai->best_action, &ai->ranked_actions[i]))
                    break;
            }
            if (i != AI_RANKED_ACTION_COUNT) {
                for (j = AI_RANKED_ACTION_COUNT - 1; i < j; j--)
                    mem_copy_u16_values(&ai->ranked_actions[j], &ai->ranked_actions[j - 1], sizeof(ai_action_data_t));
                mem_copy_u16_values(&ai->ranked_actions[j], &ai->best_action, sizeof(ai_action_data_t));
            }
        }
    }
    ai_reset_action_ranking_fields(&ai->best_action);
}

/*
 * Select an action by testing ranked entries in order.
 *
 * Each visited entry consumes a random draw, including zero-hit entries.
 * If every draw fails but any hit rate is nonzero, select the first entry.
 */
s32 ai_select_action_ranked(void) {
    ai_data_t* ai = &g_ai_data_base;
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
            mem_copy_u16_values(&ai->selected_action, &ai->ranked_actions[i], sizeof(ai_action_data_t));
            return 1;
        }
    }
    if (any == 0) {
        return 0;
    }
    mem_copy_u16_values(&ai->selected_action, &ai->ranked_actions[0], sizeof(ai_action_data_t));
    return 1;
}

/*
 * Choose or resume an action imposed by an AI-control status.
 *
 * Chicken takes precedence over Blood Suck, Confusion and Berserk. Return
 * -1 to suspend, 1 when selection completes, or 0 if none of these applies.
 */
s32 ai_select_action_status(void) {
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit;
    unit_stats_t* berserk_unit;
    ai_ability_entry_t* entry;
    s32 i, target_or_count;
    ai_coords_t coords;
    u8 status_flags;

    if (g_ai_data_base.resume_pending) {
        if (g_ai_data_base.action_selection_phase == AI_STATUS_SELECTION_CHICKEN)
            goto chicken;
        goto movement;
    }
    unit = g_ai_data_base.acting_unit;
    g_ai_data_base.movement_scenario = 0;
    status_flags = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)];
    if (status_flags & STATUS_BYTE_MASK(STATUS_ID_CHICKEN)) {
    chicken:
        if (ai_score_reachable_tiles() == -1) {
            ai->action_selection_phase = AI_STATUS_SELECTION_CHICKEN;
            return -1;
        }
        mem_zero_32bit_values(ai->tile_target_distance, sizeof(ai->tile_target_distance));
        ai_select_destination(255, AI_DESTINATION_DISTANCE_FIRST);
        coords = ai->candidate_coords[0];
        ai->selected_action.act_before_move = 0;
        goto save_movement;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_BLOOD_SUCK)] & STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)) {
        target_or_count = ai_find_nearest_target(AI_NEAREST_WITHOUT_BLOOD_SUCK);
        entry = &ai->hardcoded_status_abilities[0];
        ai->hardcoded_status_abilities[0].packed_id |= g_ai_data_base.acting_unit_id << AI_ABILITY_ENTRY_UNIT_SHIFT;
    load_targeted_ability:
        ai_load_ability_entry(entry);
        {
            /* Direct member indexing changes this reverse clear. Keep the
             * target's base cursor and derive the field offset from its member. */
            u32 cursor;
            i = BATTLE_UNIT_SLOT_COUNT - 1;
            cursor = (u32)ai + i;
            do {
                *(u8*)(cursor + ((u32)&ai->targetability.live.unit_targetable - (u32)ai)) = 0;
                i--;
                cursor--;
            } while (i >= 0);
        }
        ai->targetability.live.unit_targetable[target_or_count] = 1;
        if (ai_select_action_random())
            return 1;
    copy_target:
        ai_copy_unit_coordinates(target_or_count, &ai->considered_unit_coords);
    movement:
        if (ai_fill_target_distance_grid(&ai->considered_unit_coords) == -1) {
            ai->action_selection_phase = AI_STATUS_SELECTION_TARGET_DISTANCE;
            return -1;
        }
        ai_select_destination(0x7fffffff, AI_DESTINATION_DISTANCE_FIRST);
        coords = ai->candidate_coords[ai->movement_scenario];
        ai->selected_action.act_before_move = 0;
    save_movement:
        /* Movement-only selection leaves the other action fields unchanged. */
        ai->selected_action.command.skillset_id = 0;
        ai->selected_action.coords = coords;
        goto success;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)] & STATUS_BYTE_MASK(STATUS_ID_CONFUSION)) {
        if (ai_select_active_units(AI_UNIT_FILTER_ANY, 255)) {
            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                ai->targetability.live.unit_targetable[i] = 0;
                if (!ai_check_target_type(i))
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
                    ai_load_ability_entry(entry);
                    if (ai_select_action_random())
                        return 1;
                    target_or_count--;
                    entry->usage.policy.usable = 0;
                } while (target_or_count);
            }
        }
        target_or_count = ai_find_nearest_target(AI_NEAREST_UNIT);
        goto copy_target;
    }
    if (status_flags & STATUS_BYTE_MASK(STATUS_ID_BERSERK)) {
        target_or_count = ai_find_nearest_target(AI_NEAREST_ENEMY);
        berserk_unit = g_ai_data_base.acting_unit;
        if (berserk_unit->primary_skillset < SKILLSET_ID_MONSTER_FIRST
            && !(berserk_unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
                & STATUS_BYTE_MASK(STATUS_ID_FROG))) {
            entry = &ai->ability_lists[g_ai_data_base.acting_unit_compact_index][0];
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
 */
s32 ai_select_action_random(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 value, skillset;
    u16* abilities;
    s32 index_or_count;

    g_ai_data_base.current_action.act_before_move = 0;
    g_ai_data_base.current_action.command.skillset_id = ai->considered_ability.skillset;
    g_ai_data_base.current_action.command.ability_id = ai->considered_ability.ability_id;
    /* Item selection updates only the low byte of the 16-bit command field. */
    *(u8*)&g_ai_data_base.current_action.command.item_or_equipment = ai->considered_ability.parameters.fields.item_id;
    if (g_main_action_menu_types_by_skillset[ai->considered_ability.skillset] == ACTION_MENU_TYPE_ARITHMETICKS) {
        do {
            skillset = g_ai_math_source_skillsets[rand() % AI_MATH_SOURCE_SKILLSET_COUNT];
            abilities = main_ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ACTION);
            index_or_count = rand() % HUMAN_SKILLSET_ACTION_SLOT_COUNT;
            value = abilities[index_or_count];
        } while (value == 0 || !(g_main_ability_data[value].ai_flags.flags_4 & AI_ABILITY_FLAG_4_USABLE_BY_AI)
            || !(g_main_ability_range_data[value].flags_3 & ABILITY_SECONDARY_FLAG_3_CALCULATOR_ELIGIBLE)
            || !ai_is_ability_known(ai->acting_unit_id, skillset, index_or_count));
        ai->current_action.command.ability_id = value;
        do {
            value = rand() % CALCULATOR_ATTRIBUTE_ABILITY_COUNT;
        } while (!ai_is_ability_known(ai->acting_unit_id, ai->considered_ability.skillset, value));
        ai->current_action.command.calculator_type = value + ABILITY_ID_MATH_FIRST;
        do {
            value = rand() % CALCULATOR_MULTIPLE_ABILITY_COUNT + CALCULATOR_ATTRIBUTE_ABILITY_COUNT;
        } while (!ai_is_ability_known(ai->acting_unit_id, ai->considered_ability.skillset, value));
        ai->current_action.command.calculator_multiplier = value + ABILITY_ID_MATH_FIRST;
    }
    if (ai->considered_ability.ai_flags.word & AI_ABILITY_TARGET_MAP_TILES) {
        mem_copy_u16_values(ai->targetable_tiles, ai->reachable_tiles[0], sizeof(ai->targetable_tiles));
        ai->movable_tile_count = ai_count_flagged_tiles_within_map_bounds(ai->targetable_tiles);
        if (!ai->movable_tile_count)
            return 0;
        do {
            if (!ai_take_random_tile(ai->targetable_tiles, &ai->current_action.coords, ai->movable_tile_count))
                return 0;
            ai->movable_tile_count--;
            if (ai_is_ability_origin_allowed(&ai->current_action.coords)) {
                ai->current_action.wait_facing_hint = AI_DIRECTION_OVERLAP;
                goto selected;
            }
        } while (ai->movable_tile_count);
    } else {
        /* Removing this single-attempt loop changes register allocation. */
        do {
            if (ai->considered_ability.ai_flags.word & AI_ABILITY_FOLLOW_TARGET)
                ai->current_action.command.targeting_type = rand() % 2 + BATTLE_TARGETING_TYPE_TILE;
            else
                ai->current_action.command.targeting_type = BATTLE_TARGETING_TYPE_TILE;
            ai_build_attack_origin_masks();
            if (!ai->movable_tile_count) {
                if (ai->current_action.command.targeting_type == BATTLE_TARGETING_TYPE_UNIT) {
                    ai->current_action.command.targeting_type = BATTLE_TARGETING_TYPE_TILE;
                    ai_build_attack_origin_masks();
                }
                if (!ai->movable_tile_count)
                    break;
            }
            do {
                if (!ai_take_random_tile(ai->movable_tiles, &ai->current_action.coords, ai->movable_tile_count))
                    return 0;
                ai->movable_tile_count--;
                /* Keep the original count even when a rejected target bit is removed. */
                index_or_count = ai_calculate_ability_range();
                if (index_or_count) {
                    while (ai_take_random_tile(ai->targetable_tiles, &ai->considered_unit_coords, index_or_count)) {
                        ai->current_action.command.target_position.vx = ai->considered_unit_coords.fields.x;
                        ai->current_action.command.target_position.vz = ai->considered_unit_coords.fields.y;
                        ai->current_action.command.target_position.vy = ai->considered_unit_coords.fields.elevation;
                        ai->current_action.wait_facing_hint
                            = ai_find_direction_of_target(&ai->considered_unit_coords, &ai->current_action.coords);
                        if (ai->current_action.command.targeting_type == BATTLE_TARGETING_TYPE_UNIT
                            && (ai->current_action.command.target_id
                                   = ai_find_unit_at_coordinates(&ai->considered_unit_coords))
                                == BATTLE_UNIT_ID_NONE)
                            continue;
                        goto selected;
                    }
                }
            } while (ai->movable_tile_count);
        } while (0);
    }
    return 0;
selected: /* shared success tail */
    mem_copy_u16_values(&ai->selected_action, &ai->current_action, sizeof(ai_action_data_t));
    return 1;
}

/* Side of origin the target tile lies on, split along the two diagonals (south
 * is lower y, west lower x). A tile on a diagonal counts as south (to the
 * south-west), west (north-west), north (north-east) or east (south-east).
 * OVERLAP for the same tile. */
ai_direction_e ai_find_direction_of_target(const ai_coords_t* target, const ai_coords_t* origin) {
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

/* Intersect attack-origin tiles with the current ability's range.
 *
 * The acting unit's coordinates are restored before every return. */
s32 ai_calculate_ability_range(void) {
    ai_data_t* ai = &g_ai_data_base;
    map_tile_t* tile;
    s32 count;
    s32 level;
    s32 y;
    s32 x;
    u16* target_rows;
    u16* origin_rows;
    u16 row;
    s32 tile_mask;
    u32 level_address;
    char unused[24];

    ai_move_temp_unit_to_coords(&ai->current_action.coords);
    count = 0;
    if (ai->considered_ability.parameters.fields.range != 0) {
        if (target_set_panels_for_action(&ai->current_action.command) == TARGET_PANELS_NONE) {
            ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
            return 0;
        }
    } else {
        x = 0;
        tile = g_map_tile_data;
        do {
            x++;
            tile->ceiling_depth_and_marks.ability_range = 0;
            tile++;
        } while (x < MAP_TILE_COUNT);
        g_map_tile_data[ai->current_action.coords.fields.y * ai->map_max_x + ai->current_action.coords.fields.x
            + (ai->current_action.coords.fields.elevation * MAP_TILES_PER_LAYER)]
            .ceiling_depth_and_marks.ability_range = 1;
    }

    level = 0;
    /* Direct typed row pointers change the original loop bytes. */
    for (level_address = (u32)ai; level < MAP_TERRAIN_LAYER_COUNT;
        level++, level_address += AI_GRID_ROW_COUNT * sizeof(u16)) {
        for (y = 0; y < ai->map_max_y; y++) {
            target_rows = (u16*)(level_address + ((u32)&ai->targetable_tiles - (u32)ai));
            origin_rows = (u16*)(level_address + ((u32)&ai->attack_origin_tiles - (u32)ai));
            target_rows[y] = 0;
            row = origin_rows[y];
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    tile_mask = row << x;
                    if (tile_mask & 0x8000) {
                        if (g_map_tile_data[y * ai->map_max_x + x + (level * MAP_TILES_PER_LAYER)]
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
    ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    return count;
}

/*
 * Remove a randomly selected tile from a candidate mask.
 *
 * The supplied count determines the random ordinal. Return 0 when it is 0
 * or no corresponding set bit is found; otherwise write coordinates and return 1.
 */
s32 ai_take_random_tile(u16 tiles[MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT], ai_coords_t* coordinates, s32 count) {
    ai_data_t* ai = &g_ai_data_base;
    s32 selected;
    s32 level, y, x;
    u16 row;

    if (count == 0) {
        return 0;
    }
    selected = rand() % count;
    for (level = 0; level < MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            row = tiles[level][y];
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if ((row << x) & 0x8000) {
                        if (selected == 0) {
                            coordinates->fields.x = x;
                            coordinates->fields.y = y;
                            coordinates->fields.elevation = level;
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
s32 ai_count_flagged_tiles_within_map_bounds(u16 rows[MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT]) {
    ai_data_t* ai;
    s32 count;
    s32 level;
    s32 y;
    s32 x;
    u16 row;

    count = 0;
    ai = &g_ai_data_base;
    for (level = 0; level < MAP_TERRAIN_LAYER_COUNT; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            row = rows[level][y];
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
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
s32 ai_select_active_units(ai_unit_filter_e mode, s32 target_id) {
    s32 any_active = 0;
    s32 i;
    ai_data_t* ai = &g_ai_data_base;
    ai_unit_decision_t* decision;
    unit_stats_t* unit;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        decision = &ai->unit_decisions[i];
        ai->targetability.live.unit_active[i] = 0;
        if (decision->targeting_flags_2 & AI_TARGET_UNTARGETABLE)
            continue;
        if (!(g_battle_unit_stats[i].status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
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
            unit = &g_battle_unit_stats[i];
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
s32 ai_evaluate_ability_list(void) {
    ai_data_t* ai;
    ai_ability_entry_t* entry;

    ai = &g_ai_data_base;
    if (g_ai_data_base.resume_pending != 0) {
        goto retry; /* resume the suspended evaluation inside the loop */
    }
    g_ai_data_base.ability_counter = 0;
    do {
        entry = &ai->ability_lists[ai->acting_unit_compact_index][ai->ability_counter];
        if (entry->skillset == SKILLSET_ID_NONE) {
            return 0;
        }
        ai_load_ability_entry(entry);
    retry:
        if (ai_evaluate_ability_outcome() == -1) {
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
 * recovery. All dispatched routines otherwise collapse success to 1 while
 * preserving the suspended result -1.
 */
s32 ai_select_action_autobattle(void) {
    s32 result;

    /* A -2 result falls back to another policy's case; those jumps stay gotos. */
    switch (g_ai_data_base.autobattle_setting) {
    case AI_AUTOBATTLE_TARGETED:
        if (ai_select_action_targeted() != -1) {
            return 1;
        }
        break;
    case AI_AUTOBATTLE_RECOVERY:
    recovery_policy:
        result = ai_select_action_recovery();
        if (result == -1) {
            return -1;
        }
        if (result == -2) {
            goto retreat_policy;
        }
        return 1;
    case AI_AUTOBATTLE_FADING_LIFE:
        result = ai_select_action_fading_life();
        if (result == -1) {
            return -1;
        }
        if (result == -2) {
            goto recovery_policy;
        }
        return 1;
    case AI_AUTOBATTLE_RETREAT:
    retreat_policy:
        if (ai_select_action_retreat() != -1) {
            return 1;
        }
        break;
    default:
        return 0;
    }
    return -1;
}

/*
 * Select an AI action around the unit's designated target.
 *
 * Preserve the target across the staged movement and ability passes. Return
 * -1 to suspend and 0 when selection completes.
 */
s32 ai_select_action_targeted(void) {
    s32 target = g_ai_data_base.acting_unit->auto_battle_target;
    ai_data_t* ai = &g_ai_data_base;
    unit_stats_t* unit;
    s32 i, result;

    /* Reload the persistent target before every resume dispatch. */
    if (g_ai_data_base.resume_pending) {
        switch (g_ai_data_base.action_selection_phase) {
        case AI_TARGETED_SELECTION_SCORE_TILES:
            goto score_tiles;
        case AI_TARGETED_SELECTION_MOVEMENT:
            goto movement;
        case AI_TARGETED_SELECTION_CHARGING:
            goto charging;
        case AI_TARGETED_SELECTION_TARGET_ABILITIES:
            goto target_abilities;
        default:
            goto other_abilities;
        }
    }
    if (ai_check_target_type(target) || !ai->unit_decisions[target].enemy_flag) {
        g_ai_data_base.acting_unit_decision->flags &= ~AI_DECISION_SPECIAL_BEHAVIOR;
        unit = &g_battle_unit_stats[target];
        if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
            if (!((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                      & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))
                    && (g_battle_unit_stats[unit->mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK]
                            .status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                        & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))))
                target = unit->mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK;
        } else {
            /* A direct member pointer folds the target's base cursor and field
             * displacement together. Derive the member offset to retain its stores. */
            u32 cursor;
            s32 included = 1;
            i = BATTLE_UNIT_SLOT_COUNT - 1;
            cursor = (u32)ai + i;
            do {
                *(u8*)(cursor + ((u32)&ai->targetability.live.unit_targetable - (u32)ai)) = included;
                i--;
                cursor--;
            } while (i >= 0);
            target = ai_find_nearest_target(AI_NEAREST_ELIGIBLE_ENEMY);
            if (target == ai->acting_unit_id) {
                for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                    unit = &g_battle_unit_stats[i];
                    if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE && ai->unit_decisions[i].enemy_flag
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
    ai_focus_on_target(target);
    ai->movement_scenario = 0;
score_tiles:
    if (ai_score_reachable_tiles() == -1) {
        ai->action_selection_phase = AI_TARGETED_SELECTION_SCORE_TILES;
        return -1;
    }
movement:
    if (ai_build_targeted_movement() == -1) {
        ai->action_selection_phase = AI_TARGETED_SELECTION_MOVEMENT;
        return -1;
    }
    ai->movement_scenario = 2;
    ai_invert_target_priority();
    ai->movement_scenario = 0;
charging:
    result = ai_evaluate_charging_movement();
    switch (result) {
    case -1:
        ai->action_selection_phase = AI_TARGETED_SELECTION_CHARGING;
        return -1;
    case 1:
        return 0;
    }
    ai->movement_scenario = 0;
    if (ai_select_active_units(AI_UNIT_FILTER_SPECIFIC, target)) {
    target_abilities:
        if (ai_evaluate_ability_list() == -1) {
            ai->action_selection_phase = AI_TARGETED_SELECTION_TARGET_ABILITIES;
            return -1;
        }
        if (ai_select_action_ranked())
            return 0;
    }
    ai->movement_scenario = 1;
    if (ai_select_active_units(AI_UNIT_FILTER_ANY, 0)) {
    other_abilities:
        if (ai_evaluate_ability_list() == -1) {
            ai->action_selection_phase = AI_TARGETED_SELECTION_OTHER_ABILITIES;
            return -1;
        }
        if (ai_select_action_ranked())
            return 0;
    }
    ai->movement_scenario = 2;
    ai_select_action_fallback();
    return 0;
}

/*
 * Prepare movement scenarios around the policy's designated target.
 *
 * Return -1 while target-distance propagation is suspended, otherwise 0.
 * Recovery mode limits its initial destination search to 2 when farther away;
 * scenario 2 may approach one tile closer if the actor has not acted.
 */
s32 ai_build_targeted_movement(void) {
    ai_data_t* ai = &g_ai_data_base;
    s32 range;
    u8(*distance_rows)[AI_GRID_COLUMN_COUNT];
    u32 distance_level_base;
    s32 limit;
    s32 distance;
    s32 within_two_tiles;

    if (!g_ai_data_base.resume_pending
        && (g_ai_data_base.acting_unit->auto_battle_target == g_ai_data_base.acting_unit_id
            || ((ai->unit_decisions[g_ai_data_base.acting_unit->auto_battle_target].targeting_flags_2 >> 4)
                & (AI_TARGET_UNTARGETABLE >> 4)))) {
        mem_zero_32bit_values(ai->tile_target_distance, sizeof(ai->tile_target_distance));
    } else if (ai_fill_target_distance_grid(&ai->acting_unit_decision->target) == -1) {
        return -1;
    }
    range = ai_get_movement_range();
    /* Direct typed level indexing reverses the two address-building instructions
     * at +0xbc/+0xc0. Stage only the level base; retain the real member offset
     * and typed row/column indexing for the byte-exact match. */
    distance_level_base = ai->acting_unit_coords.fields.elevation * sizeof(ai->tile_target_distance[0]) + (u32)ai;
    distance_rows = (u8(*)[AI_GRID_COLUMN_COUNT])(distance_level_base + (u32) & ((ai_data_t*)0)->tile_target_distance);
    limit = range;
    distance = distance_rows[ai->acting_unit_coords.fields.y][ai->acting_unit_coords.fields.x];
    within_two_tiles = distance < 3;
    ai->movement_scenario = 0;
    if (ai->autobattle_setting == AI_AUTOBATTLE_RECOVERY && !within_two_tiles) {
        limit = 2;
    }
    if (ai->acting_unit_decision->targeting_flags_1 & AI_TARGETING_FLAG_1_REFLECTED_ACTION) {
        ai_select_destination(0x7fffffff, AI_DESTINATION_PRIORITY_FIRST);
    } else {
        ai_select_destination_with_nearest_fallback(limit);
    }
    ai->movement_scenario = 1;
    ai->candidate_coords[1] = ai->candidate_coords[0];
    if (!ai_filter_reachable_tiles_by_target_distance(range)) {
        ai_set_candidate_and_actor_reachable_tiles();
    }
    ai->movement_scenario = 2;
    mem_copy_u16_values(ai->reachable_tiles[2], ai->reachable_tiles[0], sizeof(ai->reachable_tiles[0]));
    if (!ai->acting_unit->action_taken) {
        s32 closer_distance = distance - 1;
        if (closer_distance < limit) {
            limit = closer_distance;
        }
        ai_select_destination_with_range_floor(limit);
    } else {
        ai->candidate_coords[2] = ai->candidate_coords[1];
    }
    ai->reachable_tiles[1][ai->acting_unit_coords.fields.elevation][ai->acting_unit_coords.fields.y]
        |= 0x8000 >> ai->acting_unit_coords.fields.x;
    return 0;
}
