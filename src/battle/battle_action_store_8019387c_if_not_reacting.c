#include "fft/battle.h"
#include "psx/types.h"

/* Store value in D_8019387C when unit_id is the acting unit of a primary
 * (non-reaction) action. */
void battle_action_store_8019387c_if_not_reacting(s32 unit_id, s8 value) {
    if (g_battle_action_context == BATTLE_ACTION_CONTEXT_PRIMARY && unit_id == g_battle_acting_unit_id) {
        D_8019387C = value;
    }
}
