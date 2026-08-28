#include "fft/battle.h"

/*
 * Predict the clockticks remaining before the death counter expires.
 *
 * Divide (counter + 1) * 100 - CT by speed, adding one for any remainder.
 * Counter zero lasts another turn; expiration may revive an Undead unit.
 * Callers reject an ability only when its CT is greater than this result.
 * The final two parameters are overwritten before use. Keeping them as
 * argument-register scratch reproduces the target's a1/a2 allocation; callers
 * supply only the unit pointer.
 */
s32 battle_ai_calculate_clockticks_until_death_counter_expires(battle_stats_t* unit, s32 total, s32 speed) {
    s32 counter;
    s32 ticks;
    s32 sum;

    speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    if (speed == 0)
        return 0x7fffffff;
    if (!(unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD)))
        return 0x7fffffff;
    if (unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_RERAISE))
        return 0x7fffffff;
    counter = unit->death_counter;
    if (counter == 0xff)
        return 0x7fffffff;
    total = counter;
    total *= 100;
    sum = total + 100;
    total = sum - unit->ct;
    ticks = total / speed;
    if (total % speed)
        ticks++;
    return ticks;
}
