#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "psx/types.h"

s32 status_apply_poison_and_regen(unit_stats_t* unit) {
    u8 status;
    if (status_check_crystal_dead_jump_petrify_treasure(unit) != 0) {
        return 0;
    }
    action_set_target_variables(unit);
    status = unit->status_sets.current[3];
    if (status & STATUS_BYTE_MASK(STATUS_ID_POISON)) {
        g_action_target_data->hp_damage = unit->max_hp >> 3;
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
    } else if (status & STATUS_BYTE_MASK(STATUS_ID_REGEN)) {
        g_action_target_data->hp_healing = unit->max_hp >> 3;
        g_action_target_data->attack_type = ACTION_TYPE_HP_HEALING;
    }
    return g_action_target_data->attack_type;
}

/* Between-turn step: makes a unit that has acted lose Transparent.
 *
 * The unit becomes the current target with a cleared action result. When it has
 * taken its action, carries Transparent as an inflicted status and has
 * transparent_removal_flag set, the result becomes a status change removing
 * Transparent. Returns the result's attack type, nonzero when there is a change
 * to show. Crystals, treasures, dead, petrified and jumping units return 0
 * untouched. */
s32 status_remove_transparent(unit_stats_t* unit) {
    unit_stats_t* target;

    if (status_check_crystal_dead_jump_petrify_treasure(unit) != 0) {
        return 0;
    }
    action_set_target_variables(unit);
    if (unit->action_taken != 0) {
        target = g_action_target;
        if ((target->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))
            != 0) {
            if (target->transparent_removal_flag != 0) {
                g_action_target_data->status_changes.removal[2] = STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT);
                g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
            }
        }
    }
    return g_action_target_data->attack_type;
}

/* Count down the unit's timed statuses, Poison through Reflect (Death Sentence
 * is not among them), and queue the removal of each that runs out. */
void status_update_expiring(s32 unit_id) {
    s32 i;
    s32 byte_idx;
    s32 mask;
    unit_stats_t* target;
    action_data_t* action;
    s32 remaining_ct;

    action_set_target_variables(&g_battle_unit_stats[unit_id]);

    /* Statuses 24..38 (bytes 3 and 4): tick the CT of each inflicted,
       non-innate status and flag it for removal when it reaches zero. */
    for (i = 0; i < 15; i++) {
        byte_idx = (i / 8) + 3;
        target = g_action_target;
        if (target->inflicted_status[byte_idx] & (mask = 0x80 >> (i & 7))) {
            if ((target->status_sets.innate[byte_idx] & mask) == 0) {
                remaining_ct = target->status_ct[i] - 1;
                target->status_ct[i] = remaining_ct;
                if ((remaining_ct & 0xFF) == 0) {
                    action = g_action_target_data;
                    action->status_changes.removal[byte_idx] = mask | action->status_changes.removal[byte_idx];
                }
            }
        }
    }

    if (status_modify_inflictions(0) != 0) {
        g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
    }
}

/* Make the unit the current target and reset its action result record. */
void action_set_target_variables(unit_stats_t* unit) {
    action_data_t* action = &unit->action;
    g_action_target = unit;
    g_action_target_data = action;
    g_action_current_ability.target_id = unit->battle_id;
    action_clear_current_data(action);
}

/* 1 for an empty slot or a unit that is a crystal, dead, in the air from Jump,
 * petrified or a treasure; otherwise 0. */
s32 status_check_crystal_dead_jump_petrify_treasure(const unit_stats_t* unit) {
    s32 result;
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return 1;
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD) | STATUS_BYTE_MASK(STATUS_ID_JUMP)))
        return 1;
    result = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE));
    if (result != 0)
        result = 1;
    return result;
}

/* Prepare Poison Marsh's automatic status action for a grounded unit.
 *
 * Mounts cannot receive the terrain effect. A rider uses the linked mount's
 * Float status because that unit determines whether the pair touches the
 * terrain. The resident terrain table marks surfaces that inflict Poison. */
s32 status_prepare_terrain_poison(unit_stats_t* unit) {
    s32 mount_info;
    u8 unit_id;
    action_data_t* action;

    mount_info = unit->mount_info;
    if (status_check_crystal_dead_jump_petrify_treasure(unit))
        return 0;
    if (mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)
        return 0;
    if (mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        if (g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK]
                .status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & STATUS_BYTE_MASK(STATUS_ID_FLOAT))
            return 0;
    } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
        return 0;
    }
    action = &unit->action;
    unit_id = unit->battle_id;
    g_action_target = unit;
    g_action_target_data = action;
    g_action_current_ability.target_id = unit_id;
    action_clear_current_data(action);
    if (g_main_terrain_status_flags[g_map_tile_data[map_calculate_location(unit)].surface.type]
        & MAIN_TERRAIN_STATUS_FLAG_POISON) {
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_POISON)]
            = STATUS_BYTE_MASK(STATUS_ID_POISON);
        if (status_modify_inflictions(0)) {
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
            return 1;
        }
    }
    return 0;
}
