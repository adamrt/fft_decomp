#include "fft/battle.h"

s32 battle_action_can_unit_react(const battle_stats_t* unit) {
    if (g_battle_action_context != BATTLE_ACTION_CONTEXT_PRIMARY) {
        return 1;
    }
    return main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_PREVENT_REACTION);
}
