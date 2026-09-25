#include "fft/battle.h"
#include "psx/types.h"

void battle_action_handle_steal_exp(battle_stats_t* unit, u8 amount) {
    s32 total;
    u8 capped;
    s16 reduced;

    if (g_battle_action_state != BATTLE_ACTION_STATE_EXECUTE) {
        return;
    }
    if ((amount & 0x80) == 0) {
        total = unit->experience + amount;
        capped = total;
        if (total >= 0x100) {
            capped = 0xFF;
        }
        unit->experience = capped;
        if (main_unit_check_level_up(unit) != 0) {
            if (unit->misc_unit_id == g_battle_acting_unit_id) {
                g_current_ability.level_gained_flag = 1;
            }
        }
    } else {
        reduced = unit->experience - (amount & 0x7F);
        if (reduced < 0) {
            reduced = 0;
        }
        unit->experience = reduced;
    }
}
