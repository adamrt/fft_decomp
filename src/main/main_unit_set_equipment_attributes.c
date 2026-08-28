#include "fft/job.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"

enum {
    ITEM_ID_MATERIA_BLADE = 0x20,
    MAX_HP_MP_CAP = 999,
};

/* Applies equipment attribute bonuses to a unit's stats.
 *
 * Clears the equipment PA/MA/SP bonuses, recomputes the base stats, then
 * folds in each equipped item's attribute record and head/body HP/MP bonuses.
 * Actual PA/MA cap at 99 and Speed at 50. */
void main_unit_set_equipment_attributes(battle_stats_t* unit, s32 level_up_check) {
    s32 i;
    s32 j;
    u32 value;
    u32 cap;
    u8 item_id;
    u32 second_id;
    item_data_t* item;
    item_attribute_t* attribute;
    u8* bonus_base;
    u8* bonus; /* equipment bonuses, later reused to walk the base/actual stats */
    u8* source;
    u8* target;

    bonus_base = unit->equipment_attributes;
    main_util_clear_byte_data(bonus_base, 3);
    cap = MAX_HP_MP_CAP;
    main_unit_calculate_actual_stats(unit, level_up_check + 1);
    unit->equipped_flags = 0;
    if ((unit->unit_flags & UNIT_FLAG_MONSTER) == 0) {
        bonus = unit->equipment_attributes;
        for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
            item_id = unit->equipment[i];
            if (item_id == ITEM_ID_MATERIA_BLADE) {
                unit->equipped_flags |= BATTLE_UNIT_EQUIPPED_FLAG_MATERIA_BLADE;
            }
            if (item_id < ITEM_ID_SHIELD_FIRST) {
                if ((u32)(g_main_item_primary_data[item_id].type - ITEM_TYPE_SWORD) < 2) {
                    unit->equipped_flags |= BATTLE_UNIT_EQUIPPED_FLAG_SWORD;
                }
            }
            if (item_id == ITEM_ID_NONE) {
                continue;
            }
            item = &g_main_item_primary_data[item_id];
            attribute = &g_main_item_attributes[item->attributes];
            source = attribute->attributes;
            for (j = 0; j < 3; j++) {
                value = source[j] + bonus[j];
                if (value >= 0x100) {
                    value = 0xff;
                }
                bonus[j] = value;
            }
            value = attribute->move + unit->move;
            if (value >= 0xfe) {
                value = 0xfd;
            }
            unit->move = value;
            value = attribute->jump + unit->jump;
            if (value >= 8) {
                value = 7;
            }
            unit->jump = value;
            j = 0;
            target = unit->status_sets.innate;
            for (; j < 15; j++) {
                *target = attribute->status_sets.innate[j] | *target;
                target++;
            }
            j = 0;
            target = unit->elemental_affinity;
            for (; j < ELEMENTAL_AFFINITY_COUNT; j++) {
                *target = attribute->elemental_affinity[j] | *target;
                target++;
            }
            if (item->type_flags & (ITEM_TYPE_FLAG_HEADGEAR | ITEM_TYPE_FLAG_BODY_ARMOR)) {
                second_id = item->secondary_data_id;
                value = unit->max_hp + g_main_item_helm_armor_secondary_data[second_id].hp_bonus;
                if (value > cap) {
                    value = cap;
                }
                unit->max_hp = value;
                value = unit->max_mp + g_main_item_helm_armor_secondary_data[second_id].mp_bonus;
                if (value > cap) {
                    /* The original routes the cap through the loop scratch; a plain
                     * `value = cap` changes the register allocation. */
                    value = j = cap;
                }
                unit->max_mp = value;
            }
        }
    }
    if (unit->hp > unit->max_hp) {
        unit->hp = unit->max_hp;
    }
    if (unit->mp > unit->max_mp) {
        unit->mp = unit->max_mp;
    }
    bonus = unit->base_attributes;
    for (j = 0; j < 3; j++) {
        value = bonus[j + 3] + bonus[j];
        if (j < 2) {
            if (value >= 100) {
                value = 99;
            }
        } else {
            if ((u16)value >= 51) {
                value = 50;
            }
        }
        bonus[j + 6] = value;
    }
}
