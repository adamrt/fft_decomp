#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Apply one stat change of an action result: the low seven bits are the amount
 * and bit 0x80 means increase. The result is clamped to min..max; returns 1
 * when the stat changed. With max 255 (CT) an amount of 0x7f stands for 255,
 * which empties or fills CT. With min 1 (Speed, PA, MA) stat is the base
 * value: the total six bytes on is rebuilt as base + equipment bonus, capped
 * at max.
 *
 * The caller's ranges: Speed 1..50, PA and MA 1..99, Brave 0..100 (10 at least
 * for a rider), Faith 0..100, CT 0..255. */
s32 battle_unit_apply_stat_increment_decrement(s32 mod, u8* stat, u8 max, u8 min) {
    s32 amount;
    s32 value;

    value = stat[0];
    amount = mod & ACTION_STAT_CHANGE_VALUE_MASK;
    if (max == 0xFF && amount == 0x7F) {
        amount = 0xFF;
    }

    if ((mod & ACTION_STAT_CHANGE_INCREASE) != 0) {
        value = value + amount;
    } else {
        value = value - amount;
    }

    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }

    if (value == stat[0]) {
        return 0;
    }
    stat[0] = value;

    if (min == 1) {
        value = stat[0] + stat[3];
        if (value > max) {
            value = max;
        }
        stat[6] = value;
    }
    return 1;
}
