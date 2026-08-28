#include "fft/battle.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

s32 battle_status_inflict_defending_to_battle_id(s32 unit_id) {
    update_unit_action_statuses(&g_battle_unit_stats[unit_id], BATTLE_STATUS_HANDLER_INDEX(BATTLE_STATUS_ID_DEFENDING));
    return 0;
}
