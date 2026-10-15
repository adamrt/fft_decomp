#include "psx/types.h"

/* Screen x of war-trophy entry `value`'s column, given the entry count
 * `index`: one column at 0xd0 for fewer than five entries, otherwise 0x9c
 * for even entries and 0x104 for odd ones. */
s32 require_reward_get_war_trophy_column_x(s32 value, s32 index) {
    s32 result = 0xd0;

    if (index >= 5) {
        s32 alternate = 0x9c;

        if (value & 1) {
            alternate = 0x104;
        }
        result = alternate;
    }
    return result;
}
