#include "fft/world.h"
#include "psx/types.h"

s32 world_item_check_two_hands_for_weapons(weapon_pair_t* slots, s32 two_hands_support) {
    s32 result;
    s32 item_id;
    s16 w0;
    s16 w1;
    u8 flags;

    result = 0;
    w0 = slots->slot_a;
    item_id = ITEM_ID_NOTHING;
    if (w0 != ITEM_ID_NOTHING) {
        if (slots->slot_b == ITEM_ID_NOTHING) {
            item_id = w0 & WORLD_ITEM_ID_MASK;
        }
    } else {
        w1 = slots->slot_b;
        if (w1 != ITEM_ID_NOTHING) {
            item_id = w1 & WORLD_ITEM_ID_MASK;
        }
    }
    if (item_id != ITEM_ID_NOTHING && item_id < ITEM_ID_THROWABLE_FIRST) {
        flags = g_main_item_weapon_data[item_id].flags;
        if (flags & WEAPON_FLAG_FORCED_TWO_HANDS) {
            result = 1;
        } else if (two_hands_support != 0) {
            if (flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE) {
                result = 1;
            }
        }
    }
    return result;
}
