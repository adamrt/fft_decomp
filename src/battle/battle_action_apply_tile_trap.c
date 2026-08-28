#include "fft/battle.h"
#include "psx/types.h"

/* Set up the action result for the trap on a unit's tile.
 *
 * A mount passes the trap to the rider in its linked partner slot. Trap 0
 * levels the target down, 1 and 2 inflict Death Sentence and Sleep (status
 * byte 4 bits 0x01 and 0x10) when battle_status_modify_inflictions allows it, and 3
 * deals max HP / 5 damage. Returns the trap id, or 0xff for none. */
s32 battle_action_apply_tile_trap(battle_stats_t* unit) {
    u8 trap;
    battle_action_data_t* action;

    trap = ((battle_move_find_result_data_t * (*)(u8, u8, u32)) battle_map_set_item_trap_data)(
        unit->x, (u8)unit->position.raw, (u32)unit->position.raw >> 15)
               ->trap_id;
    if (unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        unit = &g_battle_unit_stats[unit->mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
    }
    g_current_ability.target_id = unit->misc_unit_id;
    g_battle_action_target = unit;
    g_battle_action_target_data = &unit->action;
    battle_action_clear_current_data(&unit->action);
    switch (trap) {
    case BATTLE_TRAP_ID_DEGENERATOR:
        g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
        g_battle_action_target_data->special_effect = BATTLE_ACTION_SPECIAL_EFFECT_LEVEL_DOWN;
        break;
    case BATTLE_TRAP_ID_DEATHTRAP:
        g_battle_action_target_data->status_infliction[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_DEATH_SENTENCE)]
            = BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEATH_SENTENCE);
        if (battle_status_modify_inflictions(0) != 0) {
            g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
        } else {
            battle_formula_force_attack_miss();
        }
        break;
    case BATTLE_TRAP_ID_SLEEPING_GAS:
        g_battle_action_target_data->status_infliction[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_SLEEP)]
            = BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SLEEP);
        if (battle_status_modify_inflictions(0) != 0) {
            g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_STATUS_CHANGE;
        } else {
            battle_formula_force_attack_miss();
        }
        break;
    case BATTLE_TRAP_ID_STEEL_NEEDLE:
        action = g_battle_action_target_data;
        action->hp_damage = unit->max_hp / 5;
        action->attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
        break;
    default:
        trap = BATTLE_TRAP_ID_NONE;
        break;
    }
    return trap;
}
