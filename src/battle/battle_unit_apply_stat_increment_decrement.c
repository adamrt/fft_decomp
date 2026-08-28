#include "fft/battle.h"
#include "psx/types.h"

s32 battle_unit_apply_stat_increment_decrement(s32 mod, u8* stat, u8 max, u8 min) {
    s32 amount;
    s32 value;

    value = stat[0];
    amount = mod & BATTLE_ACTION_STAT_CHANGE_VALUE_MASK;
    if (max == 0xFF && amount == 0x7F) {
        amount = 0xFF;
    }

    if ((mod & BATTLE_ACTION_STAT_CHANGE_INCREASE) != 0) {
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
