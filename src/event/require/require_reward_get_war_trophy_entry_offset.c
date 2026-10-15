#include "psx/types.h"

/* Screen y of war-trophy row `value`, given the entry count `index`: 20
 * pixels per entry in the single column, per pair of entries in two
 * columns, from y 0x78. */
s32 require_reward_get_war_trophy_entry_offset(s32 value, s32 index) {
    if (index < 5) {
        return value * 20 + 0x78;
    }
    return (value / 2) * 20 + 0x78;
}
