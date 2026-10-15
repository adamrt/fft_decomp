#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

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
