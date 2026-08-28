#include "fft/battle.h"
#include "psx/types.h"

s32 battle_action_calculate_chance_to_react(const battle_stats_t* unit) {
    if (g_battle_action_state != BATTLE_ACTION_STATE_EXECUTE) {
        return 0;
    }
    return main_util_roll_pass_fail(0x64, unit->brave);
}
