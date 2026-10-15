#include "fft/main.h"
#include "fft/unit.h"

/* Returns 1 when the unit holds a single weapon in two hands: exactly one of
 * the two hand slots is occupied, its item is below the throwable range, and
 * the weapon either forces two hands or allows them while `two_hands_support`
 * (the Two Hands support ability) is set. */
s32 unit_is_two_handing_weapon(const s16* slots, s32 two_hands_support) {
    s32 result = 0;
    s32 item_id = 0;

    if (slots[0] != 0) {
        if (slots[1] == 0) {
            item_id = slots[0] & 0x3FF;
        }
    } else if (slots[1] != 0) {
        item_id = slots[1] & 0x3FF;
    }
    if (item_id != 0 && item_id < ITEM_ID_THROWABLE_FIRST) {
        u8 flags = g_main_item_weapon_data[item_id].flags;
        if (flags & WEAPON_FLAG_FORCED_TWO_HANDS) {
            result = 1;
        } else if (two_hands_support != 0 && (flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)) {
            result = 1;
        }
    }
    return result;
}
