#include "fft/event_bunit.h"
#include "psx/types.h"

s32 bunit_unit_is_two_handing_weapon(s16* slots, s32 allow_secondary) {
    s32 ret = 0;
    s32 id = 0;

    if (slots[0] != 0) {
        if (slots[1] == 0) {
            id = slots[0] & 0x3FF;
        }
    } else if (slots[1] != 0) {
        id = slots[1] & 0x3FF;
    }
    if (id != 0 && id < ITEM_ID_THROWABLE_FIRST) {
        u8 flags = g_main_item_weapon_data[id].flags;
        if (flags & WEAPON_FLAG_FORCED_TWO_HANDS) {
            ret = 1;
        } else if (allow_secondary != 0 && (flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)) {
            ret = 1;
        }
    }
    return ret;
}
