#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Poison takes and Regen restores an eighth of max HP, rounded down; Poison
 * wins when a unit has both. Returns the resulting action type: 0 with neither
 * status, or for a unit that is crystal, dead, jumping, petrified or treasure. */
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
