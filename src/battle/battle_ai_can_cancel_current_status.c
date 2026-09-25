#include "fft/battle.h"

/*
 * Test whether the actor's available abilities can remove this current status.
 *
 * Innate statuses are excluded. The cancellation mask aggregates the actor's
 * usable ability list; it does not describe only the considered ability.
 */
s32 battle_ai_can_cancel_current_status(battle_stats_t* unit, s32 status_bit) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 byte_off;
    s32 bit;
    s32 mask;

    byte_off = status_bit / 8;
    bit = status_bit - byte_off * 8;
    mask = 0x80;

    if (!(unit->status_sets.current[byte_off] & (mask >> bit))) {
        return 0;
    }
    if (unit->status_sets.innate[byte_off] & (mask >> bit)) {
        return 0;
    }
    if (!(ai->status_to_cancel[byte_off] & (mask >> bit))) {
        return 0;
    }
    return 1;
}
