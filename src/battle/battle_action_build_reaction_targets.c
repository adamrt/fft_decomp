#include "fft/battle.h"
#include "fft/data.h"
#include "fft/unit_slots.h"

/*
 * Build the target list for a reaction ability.
 *
 * Reflect redirects to the unit on the action tile, Distribute selects
 * injured allies, and Damage Split selects the acting unit. The remaining
 * reactions follow the target policy stored in the reaction flags table.
 */
s32 battle_action_build_reaction_targets(s32 actor_id, battle_strike_work_t* work, s32* target_count, u8* target_ids) {
    battle_stats_t* actor = &g_battle_unit_stats[actor_id];
    battle_stats_t* unit_base = g_battle_unit_stats;
    s32 tile_index;
    s32 result;
    s32 target_or_count;
    u8 flags;

    switch (g_battle_current_reaction_ability_id) {
    case ABILITY_ID_REACTION_REFLECT: {
        /* Pin: unpinned, GCC builds the target pointer in $v1 instead of $a1. */
        register battle_stats_t* target_unit __asm__("$5");
        s32 target_offset;
        u8 target_x = actor->action_target_x;
        u8 target_y;
        u8 target_elevation;
        u8 mount_info;

        work->target_new_x = target_x;
        target_y = actor->action_target_y;
        work->target_new_y = target_y;
        target_elevation = actor->action_target_elevation;
        work->target_new_map_level = target_elevation;
        target_or_count = battle_target_get_unit_id_if_tile_targetable(target_x, target_y, target_elevation);
        if (target_or_count < 0) {
            break;
        }
        /* The live offset keeps the multiply in $v0; only the final addu writes $a1. */
        target_offset = target_or_count * sizeof(*unit_base);
        target_unit = (battle_stats_t*)((u8*)unit_base + target_offset);
        __asm__("" : : "r"(target_offset));
        mount_info = target_unit->mount_info;
        if (mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
            target_or_count = mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK;
        }
        if (battle_target_is_unit_untargetable_and_store_tile(target_or_count, &tile_index) != 0) {
            break;
        }
        result = 1;
        *target_count = result;
        /* Extra uses give target_count $s1 ahead of target_ids ($s2) and unit_base ($s3). */
        __asm__("" : : "r"(target_count), "r"(target_count));
        *target_ids = target_or_count;
        goto done;
    }

    case ABILITY_ID_REACTION_DISTRIBUTE: {
        s32 unit_id;
        /* Pin: unpinned, GCC rebases the unit pointer into $a1 and reorders the loop setup. */
        register battle_stats_t* unit __asm__("$8");
        battle_stats_t* candidate;
        u8* next_target;
        s32 actor_team;
        s32 absent_slot;

        target_or_count = 0;
        unit_id = 0;
        absent_slot = 0xff;
        unit = unit_base;
        actor_team = actor->team_flags & BATTLE_TEAM_MASK;
        next_target = target_ids;
        do {
            candidate = unit;
            if (unit_id != actor_id) {
                if (candidate->entd_slot != absent_slot) {
                    if (!(candidate->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
                        if (candidate->hp != candidate->max_hp) {
                            if (!(candidate->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CRYSTAL)]
                                    & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL)
                                        | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD)
                                        | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP)))) {
                                if (!(candidate->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_PETRIFY)]
                                        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PETRIFY)
                                            | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE)))) {
                                    tile_index = candidate->initial_team_flags & BATTLE_TEAM_MASK;
                                    if (tile_index == actor_team) {
                                        *next_target++ = unit_id;
                                        target_or_count++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            unit_id++;
            unit++;
        } while (unit_id < BATTLE_UNIT_SLOT_COUNT);
        g_battle_distribute_target_count = target_or_count;
        *target_count = target_or_count;
        goto success;
    }

    case ABILITY_ID_REACTION_DAMAGE_SPLIT: {
        s32 acting_unit_id = g_battle_acting_unit_id;
        /* Pin: unpinned, GCC builds the acting-unit pointer in $v0 instead of $a1. */
        register battle_stats_t* acting_unit __asm__("$5");
        s32 acting_unit_offset;

        /* The live offset keeps the multiply in $v0; only the final addu writes $a1. */
        acting_unit_offset = acting_unit_id * sizeof(*unit_base);
        acting_unit = (battle_stats_t*)((u8*)unit_base + acting_unit_offset);
        __asm__("" : : "r"(acting_unit_offset));
        if (acting_unit->entd_slot != BATTLE_ENTD_SLOT_NONE) {
            *target_count = 1;
            *target_ids = g_battle_acting_unit_id;
            goto success;
        }
        break;
    }

    default:
        flags = g_main_reaction_behavior_flags_by_ability_id[g_battle_current_reaction_ability_id];
        if (flags & BATTLE_REACTION_BEHAVIOR_FLAG_TARGET_SELF) {
            result = 1;
            *target_count = result;
            *target_ids = actor_id;
            goto done;
        }
        if (flags & BATTLE_REACTION_BEHAVIOR_FLAG_TARGET_ATTACKER) {
            result = 0;
            goto done;
        }
        break;
    }

    /* Shared exit tail: returning directly from the cases changes the
     * target's register allocation. */
    *target_count = 0;
success:
    result = 1;
done:
    return result;
}
