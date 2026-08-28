#include "fft/battle.h"
#include "psx/types.h"

void battle_action_clear_status_changes(battle_action_data_t* action) {
    s32 i;
    for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
        g_current_ability_canceled_statuses[i] = 0;
        action->status_infliction[i] = 0;
        action->status_removal[i] = 0;
    }
}
