/* Item id list sorting and roster sorting. */

#include "fft/world.h"

/* Sort a -1 terminated item-id list in place for a shop or equipment menu.
 *
 * Entries whose id (low 10 bits) is 0 or >= 0xFE are removed first. The rest
 * are bubble-sorted by the signed-byte key list g_world_sort_key_lists[mode] (-1 ends
 * it): 0 item type (0x13 ranks as 100, ascending), 1 price, 2 ranking value
 * (a shield compared with a non-shield ranks 0), 3 physical evade, 4 magic
 * evade, 5 owned plus equipped count; keys other than 0 sort descending.
 * The magic-evade key reads 0 for the left weapon but the weapon's evade for
 * the right one; the target has this asymmetry. */
void item_sort_id_list_variant(s32 mode, s16* list) {
    s8 counts[0x100];
    s32 count;
    s32 i;
    s32 j;
    s32 passes;
    s32 k;
    s8* criteria;
    s32 criterion;
    s32 id;
    s32 item_a;
    s32 item_b;
    s32 key_a;
    s32 key_b;
    s32 diff;

    count = 0;
    while (list[count] != -1) {
        id = list[count] & 0x3ff;
        if (id == 0 || id >= ITEM_ID_END) {
            j = count;
            while ((list[j] = list[j + 1]) != -1) {
                j++;
            }
        } else {
            count++;
        }
    }
    if (count < 2) {
        return;
    }
    memset(counts, 0, 0x100);
    for (j = 0; j < g_world_formation_record_count; j++) {
        for (i = 0; i < 5; i++) {
            counts[g_world_unit_pointers[j]->equipment[i]]++;
        }
    }
    passes = count - 1;
    counts[0] = 0;
    criteria = g_item_type_order_lists[mode];
    for (; passes > 0; passes--) {
        for (j = 0; j < passes; j++) {
            k = 0;
            item_a = list[j] & 0x3ff;
            item_b = list[j + 1] & 0x3ff;
            criterion = criteria[k];
            while (criterion >= 0) {
                if (criterion == ITEM_SORT_KEY_TYPE) {
                    key_a = g_main_item_primary_data[item_a].type;
                    if (key_a == 0x13) {
                        key_a = 100;
                    }
                } else if (criterion == ITEM_SORT_KEY_PRICE) {
                    key_a = g_main_item_primary_data[item_a].price;
                } else if (criterion == ITEM_SORT_KEY_RANKING) {
                    key_a = item_get_ranking_value(item_a);
                } else if (criterion == ITEM_SORT_KEY_PHYSICAL_EVADE) {
                    if (item_a < ITEM_ID_THROWABLE_FIRST) {
                        key_a = g_main_item_weapon_data[g_main_item_primary_data[item_a].secondary_data_id].evade;
                    } else if (item_a < ITEM_ID_HEADGEAR_FIRST) {
                        key_a = g_main_item_shield_data[g_main_item_primary_data[item_a].secondary_data_id]
                                    .physical_evade;
                    } else if (item_a < ITEM_ID_CONSUMABLE_FIRST) {
                        key_a = g_main_item_accessory_data[g_main_item_primary_data[item_a].secondary_data_id]
                                    .physical_evade;
                    } else {
                        key_a = 0;
                    }
                } else if (criterion == ITEM_SORT_KEY_MAGIC_EVADE) {
                    if (item_a < ITEM_ID_THROWABLE_FIRST) {
                        key_a = 0;
                    } else if (item_a < ITEM_ID_HEADGEAR_FIRST) {
                        key_a = g_main_item_shield_data[g_main_item_primary_data[item_a].secondary_data_id].magic_evade;
                    } else if (item_a < ITEM_ID_CONSUMABLE_FIRST) {
                        key_a = g_main_item_accessory_data[g_main_item_primary_data[item_a].secondary_data_id]
                                    .magic_evade;
                    } else {
                        key_a = 0;
                    }
                } else if (criterion == ITEM_SORT_KEY_HELD_COUNT) {
                    key_a = item_adjust_inventory_count(item_a, 0) + counts[item_a];
                }
                if (criterion == ITEM_SORT_KEY_TYPE) {
                    key_b = g_main_item_primary_data[item_b].type;
                    if (key_b == 0x13) {
                        key_b = 100;
                    }
                } else if (criterion == ITEM_SORT_KEY_PRICE) {
                    key_b = g_main_item_primary_data[item_b].price;
                } else if (criterion == ITEM_SORT_KEY_RANKING) {
                    key_b = item_get_ranking_value(item_b);
                } else if (criterion == ITEM_SORT_KEY_PHYSICAL_EVADE) {
                    if (item_b < ITEM_ID_THROWABLE_FIRST) {
                        key_b = g_main_item_weapon_data[g_main_item_primary_data[item_b].secondary_data_id].evade;
                    } else if (item_b < ITEM_ID_HEADGEAR_FIRST) {
                        key_b = g_main_item_shield_data[g_main_item_primary_data[item_b].secondary_data_id]
                                    .physical_evade;
                    } else if (item_b < ITEM_ID_CONSUMABLE_FIRST) {
                        key_b = g_main_item_accessory_data[g_main_item_primary_data[item_b].secondary_data_id]
                                    .physical_evade;
                    } else {
                        key_b = 0;
                    }
                } else if (criterion == ITEM_SORT_KEY_MAGIC_EVADE) {
                    if (item_b < ITEM_ID_THROWABLE_FIRST) {
                        key_b = g_main_item_weapon_data[g_main_item_primary_data[item_b].secondary_data_id].evade;
                    } else if (item_b < ITEM_ID_HEADGEAR_FIRST) {
                        key_b = g_main_item_shield_data[g_main_item_primary_data[item_b].secondary_data_id].magic_evade;
                    } else if (item_b < ITEM_ID_CONSUMABLE_FIRST) {
                        key_b = g_main_item_accessory_data[g_main_item_primary_data[item_b].secondary_data_id]
                                    .magic_evade;
                    } else {
                        key_b = 0;
                    }
                } else {
                    key_b = item_adjust_inventory_count(item_b, 0) + counts[item_b];
                }
                k++;
                if (criterion == ITEM_SORT_KEY_RANKING) {
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
                diff = -diff;
            }
            if (diff >= 0) {
                diff = list[j];
                list[j] = list[j + 1];
                list[j + 1] = diff;
            }
        }
    }
}

/*
 * Sort the formation roster order by the key list selected by `index`,
 * then move eggs to the end in descending record-index order
 * and rebuild the unit pointer table.
 *
 * `rec` is reused for both records so that only the first record's 0x128
 * multiply leaves the key loop, as in the target. The second key value
 * shares `swap` with the egg loop and `lhs` doubles as the exchanged
 * order entry: both must be multi-set variables for the target's register
 * choice (a1/v0) and load order.
 */
void world_formation_sort_roster(s32 index) {
    s8 egg_indices[PARTY_ROSTER_SLOT_COUNT];
    s8* keys;
    s32 i;
    s32 j;
    s32 k;
    s32 count;
    s32 diff;
    s32 first;
    s32 swap;
    s32 lhs;
    s16* rec; /* Sort keys select signed 16-bit values within the unit record. */

    keys = g_item_type_order_lists[index];
    for (count = g_world_formation_record_count - 1; count > 0; count--) {
        for (i = 0; i < count; i++) {
            k = 0;
            do {
                rec = (s16*)&g_world_unit_records[g_world_formation_record_order[i]];
                lhs = rec[g_world_formation_sort_field_indices[keys[k]]];
                rec = (s16*)&g_world_unit_records[g_world_formation_record_order[i + 1]];
                swap = rec[g_world_formation_sort_field_indices[keys[k]]];
                diff = swap - lhs;
                k++;
            } while (diff == 0);
            if (keys[k - 1] != 0) {
                diff = -diff;
            }
            if (diff < 0) {
                diff = g_world_formation_record_order[i];
                lhs = (u16)g_world_formation_record_order[i + 1];
                g_world_formation_record_order[i] = lhs;
                g_world_formation_record_order[i + 1] = diff;
            }
        }
    }
    count = 0;
    for (i = 0; i < g_world_formation_record_count; i++) {
        if (g_world_unit_records[g_world_formation_record_order[i]].gender_flags & UNIT_FLAG_EGG) {
            egg_indices[count++] = g_world_formation_record_order[i];
        }
    }
    for (i = 0; i < count; i++) {
        j = 0;
        swap = egg_indices[i];
        while (1) {
            if (g_world_formation_record_order[j] == swap) {
                break;
            }
            j++;
        }
        for (; j < g_world_formation_record_count - 1; j++) {
            g_world_formation_record_order[j] = g_world_formation_record_order[j + 1];
        }
        g_world_formation_record_order[g_world_formation_record_count - 1] = egg_indices[i];
    }
    for (i = count - 1; i > 0; i--) {
        for (j = 0; j < i; j++) {
            if (egg_indices[j + 1] < egg_indices[j]) {
                first = egg_indices[j];
                egg_indices[j] = egg_indices[j + 1];
                egg_indices[j + 1] = first;
            }
        }
    }
    for (i = 0; i < count; i++) {
        g_world_formation_record_order[g_world_formation_record_count - 1 - i] = egg_indices[i];
    }
    for (i = 0; i < g_world_formation_record_count; i++) {
        g_world_unit_pointers[i] = &g_world_unit_records[g_world_formation_record_order[i]];
    }
    g_world_unit_pointers[WORLD_FORMATION_PREVIEW_UNIT_INDEX]
        = &g_world_unit_records[WORLD_FORMATION_PREVIEW_UNIT_INDEX];
}
