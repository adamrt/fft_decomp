#include "fft/battle_ai.h"
#include "psx/types.h"

/* 1 when the unit is untargetable or dead per its AI decision record;
 * otherwise the top bit of its second current-status byte. */
s32 battle_ai_check_target_type(s32 unit_id) {
    u8 flags = g_battle_ai_data_base.unit_decisions[unit_id].targeting_flags_2;

    if (((flags >> 4) & (BATTLE_AI_TARGET_UNTARGETABLE >> 4))
        || ((flags >> 5) & (BATTLE_AI_TARGET_DEAD_WITHOUT_RERAISE >> 5))) {
        return 1;
    }
    return g_battle_unit_stats[unit_id].status_sets.current[1] >> 7;
}
