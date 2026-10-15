#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Build the target list for a reaction ability.
 *
 * Reflect redirects to the unit on the action tile, Distribute selects
 * injured allies, and Damage Split selects the acting unit. The remaining
 * reactions follow the target policy stored in the reaction flags table.
 * Returns 0 only for a reaction that targets the attacker.
 */
s32 action_build_reaction_targets(s32 actor_id, battle_strike_work_t* work, s32* target_count, u8* target_ids) {
    unit_stats_t* actor = &g_battle_unit_stats[actor_id];
    unit_stats_t* unit_base = g_battle_unit_stats;
    unit_stats_t* unit;
    s32 tile_index;
    /* One variable: the Reflect target id, then the Distribute target count. */
    s32 target_or_count;
    u8 flags;

    switch (g_battle_current_reaction_ability_id) {
    case ABILITY_ID_REACTION_REFLECT: {
        u8 target_x = actor->action_command.target_position.vx;
        u8 target_y;
        u8 target_elevation;
        u8 mount_info;

        work->target_new_x = target_x;
        target_y = actor->action_command.target_position.vz;
        work->target_new_y = target_y;
        target_elevation = actor->action_command.target_position.vy;
        work->target_new_map_level = target_elevation;
        target_or_count = target_find_unit_at_tile(target_x, target_y, target_elevation);
        if (target_or_count < 0) {
            *target_count = 0;
            break;
        }
        unit = &unit_base[target_or_count];
        mount_info = unit->mount_info;
        if (mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
            target_or_count = mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK;
        }
        if (target_is_unit_untargetable_and_store_tile(target_or_count, &tile_index) != 0) {
            *target_count = 0;
            break;
        }
        *target_count = 1;
        *target_ids = target_or_count;
        break;
    }

    case ABILITY_ID_REACTION_DISTRIBUTE: {
        s32 unit_id;
        s32 actor_team;

        target_or_count = 0;
        actor_team = actor->team_flags & BATTLE_TEAM_MASK;
        for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
            if (unit_id == actor_id) {
                continue;
            }
            unit = &unit_base[unit_id];
            if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
                continue;
            }
            if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
                continue;
            }
            if (unit->hp == unit->max_hp) {
                continue;
            }
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD)
                    | STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
                continue;
            }
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
                & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
                continue;
            }
            /* Retail keeps the candidate's team in the tile-index stack slot. */
            tile_index = unit->initial_team_flags & BATTLE_TEAM_MASK;
            if (tile_index == actor_team) {
                target_ids[target_or_count++] = unit_id;
            }
        }
        g_battle_distribute_target_count = target_or_count;
        *target_count = target_or_count;
        break;
    }

    case ABILITY_ID_REACTION_DAMAGE_SPLIT:
        unit = &unit_base[g_battle_acting_unit_id];
        if (unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            *target_count = 1;
            *target_ids = g_battle_acting_unit_id;
        } else {
            *target_count = 0;
        }
        break;

    default:
        flags = g_main_reaction_behavior_flags[g_battle_current_reaction_ability_id - ABILITY_ID_REACTION_FIRST];
        if (flags & BATTLE_REACTION_BEHAVIOR_FLAG_TARGET_SELF) {
            *target_count = 1;
            *target_ids = actor_id;
            break;
        }
        if (flags & BATTLE_REACTION_BEHAVIOR_FLAG_TARGET_ATTACKER) {
            return 0;
        }
        *target_count = 0;
        break;
    }
    return 1;
}
