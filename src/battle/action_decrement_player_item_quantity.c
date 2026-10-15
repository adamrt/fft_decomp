#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Check, and optionally spend, one of an inventory item for a unit that started
 * the battle on the blue team; other teams have unlimited stock.
 *
 * Returns -1 when the party owns none, otherwise 0. One item is removed only when
 * always_one is nonzero and the action is executing (not a preview or an AI
 * simulation); with always_one 0 the call only tests the stock. */
s32 action_decrement_player_item_quantity(unit_stats_t* unit, s32 item_id, s32 always_one) {
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
        if (g_action_state != ACTION_STATE_EXECUTE) {
            return 0;
        }
        g_main_item_quantities[idx] = qty - 1;
    }
    return 0;
}
