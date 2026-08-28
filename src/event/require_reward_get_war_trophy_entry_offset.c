#include "psx/types.h"

s32 require_reward_get_war_trophy_entry_offset(s32 value, s32 index) {
    if (index < 5) {
        return value * 20 + 0x78;
    }
    return (value / 2) * 20 + 0x78;
}
