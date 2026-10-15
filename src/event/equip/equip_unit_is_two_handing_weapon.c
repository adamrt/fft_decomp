#include "fft/event_equip.h"
#include "psx/types.h"

/* Returns 1 when the unit holds a single weapon in two hands: exactly one of
 * the two hand slots is occupied, its item is below the throwable range, and
 * the weapon either forces two hands or allows them while `two_hands_support`
 * (the Two Hands support ability) is set. */
s32 equip_unit_is_two_handing_weapon(const s16* slots, s32 two_hands_support) {
    s32 result;
    s32 item_id;
    s16 w0;
    s16 w1;
    u8 flags;

    result = 0;
    w0 = slots[EQUIP_SLOT_RIGHT_HAND];
    item_id = 0;
    if (w0 != 0) {
        if (slots[EQUIP_SLOT_LEFT_HAND] == 0) {
            item_id = w0;
        }
    } else {
        w1 = slots[EQUIP_SLOT_LEFT_HAND];
        if (w1 != 0) {
            item_id = w1;
        }
    }
    if (item_id != 0 && item_id < ITEM_ID_THROWABLE_FIRST) {
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
