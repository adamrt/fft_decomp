/* The primary stat of an item's category, used to rank items in menu sorts. */

#include "fft/data_item.h"
#include "fft/item.h"
#include "fft/main_item.h"
#include "psx/types.h"

/* Return the stat the menus sort an item by, highest first: a weapon's power, a shield's
 * physical evade, the required level of an accessory or other item, and the HP bonus of
 * headgear and body armor. Item 0 falls through to the HP bonus with an index before the start
 * of the armor table (retail). */
s32 item_get_ranking_value(s32 item_id) {
    s32 type;
    s32 idx;

    /* The target narrows item_id to s16 for this s32 parameter. */
    type = item_get_menu_category(item_id);
    if (item_id != 0 && type == ITEM_MENU_CATEGORY_WEAPON) {
        return g_item_weapon_data[g_item_primary_data[item_id].secondary_data_id].power;
    }
    if (type == ITEM_MENU_CATEGORY_SHIELD) {
        return g_item_shield_data[g_item_primary_data[item_id].secondary_data_id].physical_evade;
    }
    if (type >= ITEM_MENU_CATEGORY_ACCESSORY) {
        return g_item_primary_data[item_id].required_level;
    }
    idx = item_id - ITEM_ID_HEADGEAR_FIRST;
    return g_item_helm_armor_secondary_data[idx].hp_bonus;
}
