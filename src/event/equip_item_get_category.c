#include "fft/equip.h"

/* Classify an item ID into the equipment menu's broad list category.
 *
 * Throwable weapons and consumables share the fallback category. */
item_menu_category_e equip_item_get_category(s32 item_id) {
    s32 result;

    item_id &= EQUIP_ITEM_ID_MASK;
    result = ITEM_MENU_CATEGORY_WEAPON;
    if (item_id >= ITEM_ID_THROWABLE_FIRST) {
        result = ITEM_MENU_CATEGORY_OTHER;
        if (item_id >= ITEM_ID_SHIELD_FIRST) {
            result = ITEM_MENU_CATEGORY_SHIELD;
            if (item_id >= ITEM_ID_HEADGEAR_FIRST) {
                result = ITEM_MENU_CATEGORY_HEADGEAR;
                if (item_id >= ITEM_ID_BODY_ARMOR_FIRST) {
                    if (item_id < ITEM_ID_ACCESSORY_FIRST) {
                        result = ITEM_MENU_CATEGORY_BODY_ARMOR;
                    } else {
                        result = ITEM_MENU_CATEGORY_OTHER;
                        if (item_id < ITEM_ID_CONSUMABLE_FIRST) {
                            result = ITEM_MENU_CATEGORY_ACCESSORY;
                        }
                    }
                }
            }
        }
    }
    return result;
}
