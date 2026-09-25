#include "fft/battle.h"
#include "psx/types.h"

void battle_status_remove_transparent_if_jump_used(battle_stats_t* unit) {
    battle_action_data_t* action = &g_current_action_data;
    g_battle_action_attacker_data = action;
    /* Inflicted status 3 bit 0x10 is Transparent. */
    if (unit->last_skillset_id == SKILLSET_ID_JUMP
        && (unit->inflicted_status[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TRANSPARENT))) {
        action->hit = 1;
        g_current_action_data.attack_type |= BATTLE_ACTION_TYPE_STATUS_CHANGE;
        g_current_action_data.status_removal[2]
            = g_current_action_data.status_removal[2] | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TRANSPARENT);
    }
}
