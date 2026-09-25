#include "fft/battle.h"
#include "psx/types.h"

s32 battle_ai_check_ability_use_based_on_ct(s32 unit_index) {
    s32 min_ct;
    u8 ct;

    ct = g_battle_ai_data_base.considered_ability.ct;
    if (ct != 0) {
        min_ct = battle_ai_calculate_clockticks_until_unit_acts(&g_battle_unit_stats[unit_index]);
        if (min_ct != 0x7FFFFFFF) {
            if ((s32)ct >= min_ct) {
                return battle_ai_decide_status_ct_based(ct, unit_index);
            }
        }
    }
    return 0;
}
