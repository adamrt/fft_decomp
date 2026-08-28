#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

s32 battle_action_decrement_player_item_quantity(battle_stats_t* unit, s32 item_id, s32 always_one) {
    u32 idx;
    s32 qty;

    if ((unit->initial_team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE) {
        idx = item_id & 0xFF;
        qty = g_main_item_quantities[idx];
        if (qty == 0) {
            return -1;
        }
        if (always_one == 0) {
            return 0;
        }
        if (g_battle_action_state != BATTLE_ACTION_STATE_EXECUTE) {
            return 0;
        }
        g_main_item_quantities[idx] = qty - 1;
    }
    return 0;
}
