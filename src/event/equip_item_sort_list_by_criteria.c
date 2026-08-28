#include "fft/data.h"
#include "fft/equip.h"
#include "fft/main_runtime.h"

/* Sort a -1 terminated item list in place (EQUIP twin of world_item_sort_id_list).
 *
 * Entries whose id (low 10 bits) is 0 or >= 0xFE are removed, then the rest
 * are bubble-sorted by the signed-byte criterion list
 * g_equip_item_type_order_lists[mode] (negative ends it): 0 item type (0x13
 * ranks as 100, ascending), 1 price, 2 ranking value (a shield compared with a
 * non-shield ranks 0), 3 physical evade, 4 magic evade, 5 owned count; keys
 * other than 0 sort descending. The magic-evade key reads 0 for the left
 * weapon but the weapon's evade for the right one, as in the target.
 * `diff *= -1` keeps the negation in int on the sign-extended difference, and
 * the offset-first address sum keeps the target's addu operand order. */
void equip_item_sort_list_by_criteria(s32 mode, s16* list) {
    s16 count;
    s16* entry;
    s32 j;
    s32 passes;
    s32 k;
    s16 item_a;
    s16 item_b;
    u8* criteria;
    s16 criterion;
    s32 key_a;
    s32 key_b;
    s16 diff;

    count = 0;
    while (list[count] != -1) {
        entry = (s16*)(count * 2 + (s32)list);
        item_a = *entry & 0x3ff;
        if (item_a == 0 || item_a >= 0xfe) {
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
    criteria = g_equip_item_type_order_lists[mode];
    for (; passes > 0; passes--) {
        for (j = 0; j < passes; j++) {
            k = 0;
            criterion = (s8)criteria[k];
            item_a = list[j] & 0x3ff;
            item_b = list[j + 1] & 0x3ff;
            while (criterion >= 0) {
                if (criterion == 0) {
                    key_a = g_main_item_primary_data[item_a].type;
                    if (key_a == 0x13) {
                        key_a = 100;
                    }
                } else if (criterion == 1) {
                    key_a = g_main_item_primary_data[item_a].price;
                } else if (criterion == 2) {
                    key_a = equip_item_get_ranking_value(item_a);
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
                    key_a = equip_item_get_total_count(item_a);
                }
                if (criterion == 0) {
                    key_b = g_main_item_primary_data[item_b].type;
                    if (key_b == 0x13) {
                        key_b = 100;
                    }
                } else if (criterion == 1) {
                    key_b = g_main_item_primary_data[item_b].price;
                } else if (criterion == 2) {
                    key_b = equip_item_get_ranking_value(item_b);
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
                    key_b = equip_item_get_total_count(item_b);
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
                criterion = (s8)criteria[k];
            }
            if (criterion == 0) {
                diff *= -1;
            }
            if (diff >= 0) {
                diff = list[j];
                list[j] = list[j + 1];
                list[j + 1] = diff;
            }
        }
    }
}
