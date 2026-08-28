#include "fft/job.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "psx/types.h"

void main_unit_set_equipment_stats(battle_stats_t* unit) {
    item_data_t* item;
    shield_data_t* shield;
    weapon_data_t* weapon;

    main_util_clear_byte_data(unit->equipment_stats, BATTLE_UNIT_EQUIPMENT_STAT_COUNT);
    unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE]
        = g_job_data_pointer[unit->job_id].class_evade;
    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        return;
    }

    item = &g_main_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]];
    if (item->type_flags & ITEM_TYPE_FLAG_ACCESSORY) {
        accessory_data_t* g_main_item_accessory_flags
            = &g_main_item_accessory_data[g_main_item_secondary_data_ids[unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]
                * sizeof(item_data_t)]];
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE]
            = g_main_item_accessory_flags->physical_evade;
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE]
            = g_main_item_accessory_flags->magic_evade;
    }
    item = &g_main_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]];
    if (item->type_flags & ITEM_TYPE_FLAG_SHIELD) {
        shield = &g_main_item_shield_data[g_main_item_secondary_data_ids
                [unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] * sizeof(item_data_t)]];
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE] = shield->physical_evade;
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE] = shield->magic_evade;
    }
    item = &g_main_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]];
    if (item->type_flags & ITEM_TYPE_FLAG_SHIELD) {
        shield = &g_main_item_shield_data[g_main_item_secondary_data_ids
                [unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] * sizeof(item_data_t)]];
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE] = shield->physical_evade;
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE] = shield->magic_evade;
    }
    item = &g_main_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]];
    if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
        weapon = &g_main_item_weapon_data[unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]];
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER] = weapon->power;
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE] = weapon->evade;
    }
    item = &g_main_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]];
    if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
        weapon = &g_main_item_weapon_data[unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]];
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER] = weapon->power;
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE] = weapon->evade;
    }
}
