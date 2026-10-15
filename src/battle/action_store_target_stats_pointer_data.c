#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/types.h"

/* Apply the prepared action result to one target: make the unit the current
 * target, copy the 0x2c-byte result from g_action_attacker_action_data
 * into its action record and finalize it with
 * action_finalize_attack_and_flag_reactions.
 *
 * Returns that routine's result, or -1 for an invalid or empty slot. A result
 * that is not a hit is marked as canceled. */
s32 action_store_target_stats_pointer_data(s32 unit_id) {
    unit_stats_t* stats;
    action_data_t* action;
    s32 result;

    stats = &g_battle_unit_stats[unit_id];
    g_action_target = stats;
    if (unit_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }
    action = &g_battle_unit_stats[unit_id].action;
    g_action_current_ability.target_id = unit_id;
    g_action_target_data = action;
    if (stats->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    mem_copy_byte_data_source_first(&g_action_attacker_action_data, action, 0x2C);
    result = action_finalize_attack_and_flag_reactions(unit_id);
    if (g_action_target_data->hit == 0) {
        g_action_target_data->miss_type = ACTION_MISS_TYPE_CANCELED;
    }
    return result;
}
