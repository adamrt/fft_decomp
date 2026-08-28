#include "fft/battle_ai.h"

/*
 * Test whether a current status lasts through the requested charge time.
 *
 * Equality counts as present. Jump uses its charged-ability timer; other
 * timed statuses use their duration unless the status is innate.
 */
s32 battle_ai_is_status_active_through_delay(s32 delay, battle_stats_t* unit, s32 status_id) {
    s32 byte_off;

    byte_off = status_id / 8;

    if (!(unit->status_sets.current[byte_off] & (0x80 >> (status_id - byte_off * 8)))) {
        return 0;
    }
    if (status_id == BATTLE_STATUS_ID_JUMP) {
        if (unit->charged_ability_ct < delay) {
            return 0;
        }
    } else {
        if (status_id < BATTLE_STATUS_ID_POISON) {
            return 1;
        }
        if (unit->status_sets.innate[byte_off] & (0x80 >> (status_id - byte_off * 8))) {
            return 1;
        }
        if (unit->status_ct[BATTLE_STATUS_CT_INDEX(status_id)] < delay) {
            return 0;
        }
    }
    return 1;
}
