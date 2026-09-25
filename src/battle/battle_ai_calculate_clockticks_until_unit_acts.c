#include "fft/battle.h"
#include "psx/types.h"

s32 battle_ai_calculate_clockticks_until_unit_acts(battle_stats_t* unit) {
    battle_ai_data_t* ai;
    s32 speed;
    s32 ticks;
    s32 rem;

    ai = &g_battle_ai_data_base;
    speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    ticks = 0;
    if (speed == 0) {
        return 0x7FFFFFFF;
    }
    if ((*(u16*)&unit->status_sets.current[0]
            & (BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_CRYSTAL) | BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_PETRIFY)
                | BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_TREASURE)))
        != 0) {
        return 0x7FFFFFFF;
    }
    if ((ai->unit_decisions[unit->entd_slot].targeting_flags_2 >> 5) & (BATTLE_AI_TARGET_DEAD_WITHOUT_RERAISE >> 5)) {
        return 0x7FFFFFFF;
    }
    if (unit->status_sets.current[3] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_STOP)) {
        ticks = unit->status_ct[BATTLE_STATUS_CT_INDEX(BATTLE_STATUS_ID_STOP)];
    }
    if (unit->ct < 0x64) {
        rem = 0x64 - unit->ct;
        ticks += rem / speed;
        if (rem % speed != 0) {
            ticks += 1;
        }
    }
    if (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD)) {
        return ticks;
    }
    if ((unit->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SLEEP)) == 0) {
        return ticks;
    }
    if (ticks < unit->status_ct[BATTLE_STATUS_CT_INDEX(BATTLE_STATUS_ID_SLEEP)]) {
        ticks = unit->status_ct[BATTLE_STATUS_CT_INDEX(BATTLE_STATUS_ID_SLEEP)];
    }
    return ticks;
}
