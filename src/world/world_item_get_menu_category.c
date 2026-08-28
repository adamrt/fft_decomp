#include "fft/world.h"

/* Classify an item ID into the WORLD equipment menu's broad list category.
 *
 * Throwable weapons and consumables share category 5. */
item_menu_category_e world_item_get_menu_category(s32 item_id) {
    s32 category;

    item_id &= WORLD_ITEM_ID_MASK;
    category = ITEM_MENU_CATEGORY_WEAPON;
    if (item_id >= ITEM_ID_THROWABLE_FIRST) {
        category = ITEM_MENU_CATEGORY_OTHER;
        if (item_id >= ITEM_ID_SHIELD_FIRST) {
            category = ITEM_MENU_CATEGORY_SHIELD;
            if (item_id >= ITEM_ID_HEADGEAR_FIRST) {
                category = ITEM_MENU_CATEGORY_HEADGEAR;
                if (item_id >= ITEM_ID_BODY_ARMOR_FIRST) {
                    if (item_id < ITEM_ID_ACCESSORY_FIRST) {
                        category = ITEM_MENU_CATEGORY_BODY_ARMOR;
                    } else if (item_id < ITEM_ID_CONSUMABLE_FIRST) {
                        category = ITEM_MENU_CATEGORY_ACCESSORY;
                    } else {
                        category = ITEM_MENU_CATEGORY_OTHER;
                    }
                }
            }
        }
    }
    return category;
}
