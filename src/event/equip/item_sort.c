/* Sorting an item list by a mode's list of criteria, dropping empty and invalid entries. */

#include "fft/item.h"
#include "fft/item_menu.h"
#include "fft/menu_sort.h"
#include "psx/types.h"

/* EQUIP uses the five editable item-category orders in resident MAIN storage.
 * Slot 0 is unused; WORLD has a separate directory with Formation and fixed orders. */
s8* const g_item_type_order_lists[6] = {
    0,
    g_menu_sort_orders.weapons_and_shields,
    g_menu_sort_orders.headgear,
    g_menu_sort_orders.body_armor,
    g_menu_sort_orders.accessories,
    g_menu_sort_orders.other_items,
};

/* Sort a -1 terminated item list in place (EQUIP's body).
 *
 * Entries whose id (low 10 bits) is 0 or >= ITEM_ID_END are removed, then the
 * rest are bubble-sorted by the signed-byte criterion list
 * g_item_type_order_lists[mode] (negative ends it): 0 item type (shields rank
 * as 100, ascending), 1 price, 2 ranking value (a shield compared with a
 * non-shield ranks 0), 3 physical evade, 4 magic evade, 5 owned count; keys
 * other than 0 sort descending. The magic-evade key reads 0 for a weapon in
 * the earlier entry (item_a) but the weapon's evade in the later one (item_b),
 * as in the target. The evade keys read the shield records for the throwing
 * items too and the accessory records for headgear and body armor, by their
 * secondary data ids (see QUIRKS.md); the ranking's shield test also takes the
 * throwing items for shields.
 * `diff *= -1` keeps the negation in int on the sign-extended difference, and
 * the offset-first address sum keeps the target's addu operand order. */
void item_id_list_sort(s32 mode, s16* list) {
    s16 count = 0;
    s16* entry;
    s32 j;
    s32 passes;
    s32 k;
    s16 item_a;
    s16 item_b;
    s8* criteria;
    s16 criterion;
    s32 key_a;
    s32 key_b;
    s16 diff;

    while (list[count] != -1) {
        entry = (s16*)(count * 2 + (s32)list);
        item_a = *entry & ITEM_ENTRY_ID_MASK;
        if (item_a == 0 || item_a >= ITEM_ID_END) {
            while ((*entry = entry[1]) != -1) {
                entry++;
            }
        } else {
            count++;
        }
    }

    if (count < 2) {
        return;
    }

    passes = count - 1;
    criteria = g_item_type_order_lists[mode];
    for (; passes > 0; passes--) {
        for (j = 0; j < passes; j++) {
            k = 0;
            criterion = criteria[k];
            item_a = list[j] & ITEM_ENTRY_ID_MASK;
            item_b = list[j + 1] & ITEM_ENTRY_ID_MASK;

            while (criterion >= 0) {
                if (criterion == ITEM_SORT_KEY_TYPE) {
                    key_a = g_item_primary_data[item_a].type;
                    if (key_a == ITEM_TYPE_SHIELD) {
                        key_a = 100;
                    }
                } else if (criterion == ITEM_SORT_KEY_PRICE) {
                    key_a = g_item_primary_data[item_a].price;
                } else if (criterion == ITEM_SORT_KEY_RANKING) {
                    key_a = item_ranking_value_get(item_a);
                } else if (criterion == ITEM_SORT_KEY_PHYSICAL_EVADE) {
                    if (item_a < ITEM_ID_THROWABLE_FIRST) {
                        key_a = g_item_weapon_data[g_item_primary_data[item_a].secondary_data_id].evade;
                    } else if (item_a < ITEM_ID_HEADGEAR_FIRST) {
                        key_a = g_item_shield_data[g_item_primary_data[item_a].secondary_data_id].physical_evade;
                    } else if (item_a < ITEM_ID_CONSUMABLE_FIRST) {
                        key_a = g_item_accessory_data[g_item_primary_data[item_a].secondary_data_id].physical_evade;
                    } else {
                        key_a = 0;
                    }
                } else if (criterion == ITEM_SORT_KEY_MAGIC_EVADE) {
                    if (item_a < ITEM_ID_THROWABLE_FIRST) {
                        key_a = 0;
                    } else if (item_a < ITEM_ID_HEADGEAR_FIRST) {
                        key_a = g_item_shield_data[g_item_primary_data[item_a].secondary_data_id].magic_evade;
                    } else if (item_a < ITEM_ID_CONSUMABLE_FIRST) {
                        key_a = g_item_accessory_data[g_item_primary_data[item_a].secondary_data_id].magic_evade;
                    } else {
                        key_a = 0;
                    }
                } else if (criterion == ITEM_SORT_KEY_HELD_COUNT) {
                    key_a = item_total_count_get(item_a);
                }

                if (criterion == ITEM_SORT_KEY_TYPE) {
                    key_b = g_item_primary_data[item_b].type;
                    if (key_b == ITEM_TYPE_SHIELD) {
                        key_b = 100;
                    }
                } else if (criterion == ITEM_SORT_KEY_PRICE) {
                    key_b = g_item_primary_data[item_b].price;
                } else if (criterion == ITEM_SORT_KEY_RANKING) {
                    key_b = item_ranking_value_get(item_b);
                } else if (criterion == ITEM_SORT_KEY_PHYSICAL_EVADE) {
                    if (item_b < ITEM_ID_THROWABLE_FIRST) {
                        key_b = g_item_weapon_data[g_item_primary_data[item_b].secondary_data_id].evade;
                    } else if (item_b < ITEM_ID_HEADGEAR_FIRST) {
                        key_b = g_item_shield_data[g_item_primary_data[item_b].secondary_data_id].physical_evade;
                    } else if (item_b < ITEM_ID_CONSUMABLE_FIRST) {
                        key_b = g_item_accessory_data[g_item_primary_data[item_b].secondary_data_id].physical_evade;
                    } else {
                        key_b = 0;
                    }
                } else if (criterion == ITEM_SORT_KEY_MAGIC_EVADE) {
                    /* A weapon's key here is its evade, where key_a above reads 0 (retail asymmetry). */
                    if (item_b < ITEM_ID_THROWABLE_FIRST) {
                        key_b = g_item_weapon_data[g_item_primary_data[item_b].secondary_data_id].evade;
                    } else if (item_b < ITEM_ID_HEADGEAR_FIRST) {
                        key_b = g_item_shield_data[g_item_primary_data[item_b].secondary_data_id].magic_evade;
                    } else if (item_b < ITEM_ID_CONSUMABLE_FIRST) {
                        key_b = g_item_accessory_data[g_item_primary_data[item_b].secondary_data_id].magic_evade;
                    } else {
                        key_b = 0;
                    }
                } else {
                    key_b = item_total_count_get(item_b);
                }

                k++;
                if (criterion == ITEM_SORT_KEY_RANKING) {
                    /* `diff` first collects which items are shields (1: item_a, 2: item_b); a shield
                     * beside a non-shield ranks 0. */
                    diff = 0;
                    if (item_a >= ITEM_ID_THROWABLE_FIRST && item_a < ITEM_ID_HEADGEAR_FIRST) {
                        diff = 1;
                    }
                    if (item_b >= ITEM_ID_THROWABLE_FIRST && item_b < ITEM_ID_HEADGEAR_FIRST) {
                        diff |= 2;
                    }
                    if (diff == 1) {
                        key_a = 0;
                    }
                    if (diff == 2) {
                        key_b = 0;
                    }
                }

                diff = key_b - key_a;
                if (diff != 0) {
                    break;
                }
                criterion = criteria[k];
            }

            if (criterion == ITEM_SORT_KEY_TYPE) {
                diff *= -1;
            }

            /* Equal entries swap too, so ties do not keep their order; `diff` then serves as the swap
             * temporary. */
            if (diff >= 0) {
                diff = list[j];
                list[j] = list[j + 1];
                list[j + 1] = diff;
            }
        }
    }
}
