#include "fft/battle_action.h"
#include "fft/battle_status.h"

/* Makes the attacker's own action result remove Transparent when its last
 * skillset is Jump and it carries Transparent as an inflicted status. Also points
 * g_action_attacker_data at the attacker's result. */
void status_remove_transparent_if_jump_used(unit_stats_t* unit) {
    action_data_t* action = &g_action_attacker_action_data;
    g_action_attacker_data = action;
    /* Inflicted status byte 2 bit 0x10 is Transparent. */
    if (unit->action_command.skillset_id == SKILLSET_ID_JUMP
        && (unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
            & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))) {
        action->hit = 1;
        g_action_attacker_action_data.attack_type |= ACTION_TYPE_STATUS_CHANGE;
        g_action_attacker_action_data.status_changes.removal[2]
            = g_action_attacker_action_data.status_changes.removal[2] | STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT);
    }
}
