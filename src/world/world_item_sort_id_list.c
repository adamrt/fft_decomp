#include "fft/world.h"
#include "psx/libc.h"

/* Sort a -1 terminated item-id list in place for a shop or equipment menu.
 *
 * Entries whose id (low 10 bits) is 0 or >= 0xFE are removed first. The rest
 * are bubble-sorted by the signed-byte key list g_world_sort_key_lists[mode] (-1 ends
 * it): 0 item type (0x13 ranks as 100, ascending), 1 price, 2 ranking value
 * (a shield compared with a non-shield ranks 0), 3 physical evade, 4 magic
 * evade, 5 owned plus equipped count; keys other than 0 sort descending.
 * The magic-evade key reads 0 for the left weapon but the weapon's evade for
 * the right one; the target has this asymmetry. */
void world_item_sort_id_list(s32 mode, world_item_list_entry_t* list) {
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
    while (list[count].value != -1) {
        id = list[count].value & 0x3ff;
        if (id == 0 || id >= 0xfe) {
            j = count;
            while ((list[j].value = list[j + 1].value) != -1) {
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
            counts[g_world_formation_unit_pointers[j]->equipment[i]]++;
        }
    }
    passes = count - 1;
    counts[0] = 0;
    criteria = g_world_sort_key_lists[mode];
    for (; passes > 0; passes--) {
        for (j = 0; j < passes; j++) {
            k = 0;
            item_a = list[j].value & 0x3ff;
            item_b = list[j + 1].value & 0x3ff;
            criterion = criteria[k];
            while (criterion >= 0) {
                if (criterion == 0) {
                    key_a = g_main_item_primary_data[item_a].type;
                    if (key_a == 0x13) {
                        key_a = 100;
                    }
                } else if (criterion == 1) {
                    key_a = g_main_item_primary_data[item_a].price;
                } else if (criterion == 2) {
                    /* No u8 mask on the result: called as returning int. */
                    key_a = ((s32 (*)(s32))world_item_get_ranking_value)(item_a);
                } else if (criterion == 3) {
                    if (item_a < 0x7a) {
                        key_a = g_main_item_weapon_data[g_main_item_primary_data[item_a].secondary_data_id].evade;
                    } else if (item_a < 0x90) {
                        key_a = g_main_item_shield_data[g_main_item_primary_data[item_a].secondary_data_id]
                                    .physical_evade;
                    } else if (item_a < 0xf0) {
                        key_a = g_main_item_accessory_data[g_main_item_primary_data[item_a].secondary_data_id]
                                    .physical_evade;
                    } else {
                        key_a = 0;
                    }
                } else if (criterion == 4) {
                    if (item_a < 0x7a) {
                        key_a = 0;
                    } else if (item_a < 0x90) {
                        key_a = g_main_item_shield_data[g_main_item_primary_data[item_a].secondary_data_id].magic_evade;
                    } else if (item_a < 0xf0) {
                        key_a = g_main_item_accessory_data[g_main_item_primary_data[item_a].secondary_data_id]
                                    .magic_evade;
                    } else {
                        key_a = 0;
                    }
                } else if (criterion == 5) {
                    key_a = world_item_change_quantity_on_equip(item_a, 0) + counts[item_a];
                }
                if (criterion == 0) {
                    key_b = g_main_item_primary_data[item_b].type;
                    if (key_b == 0x13) {
                        key_b = 100;
                    }
                } else if (criterion == 1) {
                    key_b = g_main_item_primary_data[item_b].price;
                } else if (criterion == 2) {
                    /* No u8 mask on the result: called as returning int. */
                    key_b = ((s32 (*)(s32))world_item_get_ranking_value)(item_b);
                } else if (criterion == 3) {
                    if (item_b < 0x7a) {
                        key_b = g_main_item_weapon_data[g_main_item_primary_data[item_b].secondary_data_id].evade;
                    } else if (item_b < 0x90) {
                        key_b = g_main_item_shield_data[g_main_item_primary_data[item_b].secondary_data_id]
                                    .physical_evade;
                    } else if (item_b < 0xf0) {
                        key_b = g_main_item_accessory_data[g_main_item_primary_data[item_b].secondary_data_id]
                                    .physical_evade;
                    } else {
                        key_b = 0;
                    }
                } else if (criterion == 4) {
                    if (item_b < 0x7a) {
                        key_b = g_main_item_weapon_data[g_main_item_primary_data[item_b].secondary_data_id].evade;
                    } else if (item_b < 0x90) {
                        key_b = g_main_item_shield_data[g_main_item_primary_data[item_b].secondary_data_id].magic_evade;
                    } else if (item_b < 0xf0) {
                        key_b = g_main_item_accessory_data[g_main_item_primary_data[item_b].secondary_data_id]
                                    .magic_evade;
                    } else {
                        key_b = 0;
                    }
                } else {
                    key_b = world_item_change_quantity_on_equip(item_b, 0) + counts[item_b];
                }
                k++;
                if (criterion == 2) {
                    diff = 0;
                    if (item_a >= 0x7a && item_a < 0x90) {
                        diff = 1;
                    }
                    if (item_b >= 0x7a && item_b < 0x90) {
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
            if (criterion == 0) {
                diff = -diff;
            }
            if (diff >= 0) {
                diff = list[j].value;
                list[j].value = list[j + 1].value;
                list[j + 1].value = diff;
            }
        }
    }
}
