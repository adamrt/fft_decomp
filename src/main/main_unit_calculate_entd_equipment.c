#include "fft/main.h"
#include "psx/types.h"

void main_unit_calculate_entd_equipment(battle_stats_t* unit_arg, entd_unit_t* entd) {
    battle_stats_t* unit = unit_arg;
    u8 item;
    u8 flags;
    s32 i;

    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        main_unit_store_monster_equipment(unit, entd);
        return;
    }

    /* Right hand */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_RIGHT_HAND];
    flags = unit->support_abilities[2];
    if (item == ENTD_EQUIPMENT_RANDOM) {
        if (flags & BATTLE_SUPPORT_SET_3_TWO_HANDS) {
            item = main_unit_calculate_random_equipment(
                unit, ITEM_TYPE_FLAG_WEAPON, WEAPON_FLAG_TWO_HANDS_COMPATIBLE, ITEM_TYPE_FILTER_ANY);
        }
        if (item == ENTD_EQUIPMENT_RANDOM && (flags & BATTLE_SUPPORT_SET_3_TWO_SWORDS)) {
            item = main_unit_calculate_random_equipment(
                unit, ITEM_TYPE_FLAG_WEAPON, WEAPON_FLAG_TWO_SWORDS_COMPATIBLE, ITEM_TYPE_FILTER_ANY);
        }
    }
    flags = unit->support_abilities[0];
    if (item == ENTD_EQUIPMENT_RANDOM) {
        if (flags & BATTLE_SUPPORT_SET_1_EQUIP_SWORD) {
            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_SWORD);
        }
        if (item == ENTD_EQUIPMENT_RANDOM) {
            if (flags & BATTLE_SUPPORT_SET_1_EQUIP_KATANA) {
                item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_KATANA);
            }
            if (item == ENTD_EQUIPMENT_RANDOM) {
                if (flags & BATTLE_SUPPORT_SET_1_EQUIP_CROSSBOW) {
                    item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_CROSSBOW);
                }
                if (item == ENTD_EQUIPMENT_RANDOM) {
                    if (flags & BATTLE_SUPPORT_SET_1_EQUIP_SPEAR) {
                        item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_POLEARM);
                    }
                    if (item == ENTD_EQUIPMENT_RANDOM) {
                        if (flags & BATTLE_SUPPORT_SET_1_EQUIP_AXE) {
                            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_AXE);
                        }
                        if (item == ENTD_EQUIPMENT_RANDOM) {
                            if (flags & BATTLE_SUPPORT_SET_1_EQUIP_GUN) {
                                item = main_unit_calculate_random_equipment(
                                    unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_GUN);
                            }
                            if (item == ENTD_EQUIPMENT_RANDOM) {
                                item = main_unit_calculate_random_equipment(
                                    unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_FILTER_ANY);
                                if (item == ENTD_EQUIPMENT_RANDOM) {
                                    item = ITEM_ID_NONE;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (g_main_item_primary_data[item].type_flags & ITEM_TYPE_FLAG_SHIELD) {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item;
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
    } else {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item;
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = ITEM_ID_NONE;
    }

    /* Left hand */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_LEFT_HAND];
    if (unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
        && ((g_main_item_weapon_data[g_main_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]]
                                         .secondary_data_id]
                    .flags
                & WEAPON_FLAG_FORCED_TWO_HANDS)
            || (unit->support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS))) {
        item = ITEM_ID_NONE;
    }
    flags = unit->support_abilities[2];
    if (item == ENTD_EQUIPMENT_RANDOM && (flags & BATTLE_SUPPORT_SET_3_TWO_SWORDS)) {
        item = main_unit_calculate_random_equipment(
            unit, ITEM_TYPE_FLAG_WEAPON, WEAPON_FLAG_TWO_SWORDS_COMPATIBLE, ITEM_TYPE_FILTER_ANY);
    }
    flags = unit->equipment_categories[EQUIPPABLE_ITEM_TYPE_BYTE_INDEX(ITEM_TYPE_SHIELD)];
    if (item == ENTD_EQUIPMENT_RANDOM) {
        if (flags & EQUIPPABLE_ITEM_TYPE_BYTE_MASK(ITEM_TYPE_SHIELD)) {
            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_SHIELD, 0, ITEM_TYPE_FILTER_ANY);
        } else {
            item = ITEM_ID_NONE;
        }
    }
    if (g_main_item_primary_data[item].type_flags & ITEM_TYPE_FLAG_WEAPON) {
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item;
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = ITEM_ID_NONE;
    } else {
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item;
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
    }

    /* Head */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_HEAD];
    flags = unit->support_abilities[0];
    if (item == ENTD_EQUIPMENT_RANDOM) {
        if (flags & BATTLE_SUPPORT_SET_1_EQUIP_ARMOR) {
            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_HEADGEAR, 0, ITEM_TYPE_HELMET);
        }
        if (item == ENTD_EQUIPMENT_RANDOM) {
            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_HEADGEAR, 0, ITEM_TYPE_FILTER_ANY);
            if (item == ENTD_EQUIPMENT_RANDOM) {
                item = ITEM_ID_NONE;
            }
        }
    }
    unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD] = item;

    /* Body */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_BODY];
    if (item == ENTD_EQUIPMENT_RANDOM) {
        /* The target reuses Equip Shield here while requesting random Armor;
         * preserve this native equipment-generation quirk. */
        if (flags & BATTLE_SUPPORT_SET_1_EQUIP_SHIELD) {
            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_BODY_ARMOR, 0, ITEM_TYPE_ARMOR);
        }
        if (item == ENTD_EQUIPMENT_RANDOM) {
            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_BODY_ARMOR, 0, ITEM_TYPE_FILTER_ANY);
            if (item == ENTD_EQUIPMENT_RANDOM) {
                item = ITEM_ID_NONE;
            }
        }
    }
    unit->equipment[UNIT_EQUIPMENT_SLOT_BODY] = item;

    /* Accessory */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_ACCESSORY];
    if (item == ENTD_EQUIPMENT_RANDOM) {
        item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_ACCESSORY, 0, ITEM_TYPE_FILTER_ANY);
        if (item == ENTD_EQUIPMENT_RANDOM) {
            item = ITEM_ID_NONE;
        }
    }
    unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY] = item;

    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        if (unit->equipment[i] == ITEM_ID_NOTHING) {
            unit->equipment[i] = ITEM_ID_NONE;
        }
    }
}
