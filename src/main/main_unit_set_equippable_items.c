#include "fft/main.h"

void main_unit_set_equippable_items(battle_stats_t* unit) {
    s32 support = unit->support_abilities[0];

    if (support & BATTLE_SUPPORT_SET_1_EQUIP_ARMOR) {
        unit->equipment_categories[2] |= BATTLE_EQUIPMENT_SET_3_HELM | BATTLE_EQUIPMENT_SET_3_ARMOR;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_SHIELD) {
        unit->equipment_categories[2] |= BATTLE_EQUIPMENT_SET_3_SHIELD;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_SWORD) {
        unit->equipment_categories[0] |= BATTLE_EQUIPMENT_SET_1_SWORD;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_KATANA) {
        unit->equipment_categories[0] |= BATTLE_EQUIPMENT_SET_1_KATANA;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_CROSSBOW) {
        unit->equipment_categories[1] |= BATTLE_EQUIPMENT_SET_2_CROSSBOW;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_SPEAR) {
        unit->equipment_categories[1] |= BATTLE_EQUIPMENT_SET_2_SPEAR;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_AXE) {
        unit->equipment_categories[0] |= BATTLE_EQUIPMENT_SET_1_AXE;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_GUN) {
        unit->equipment_categories[1] |= BATTLE_EQUIPMENT_SET_2_GUN;
    }
    support = unit->unit_flags;
    if (support & UNIT_FLAG_FEMALE) {
        unit->equipment_categories[2] |= BATTLE_EQUIPMENT_SET_3_HAIR_ORNAMENT | BATTLE_EQUIPMENT_SET_3_BAG;
        unit->equipment_categories[3] |= BATTLE_EQUIPMENT_SET_4_PERFUME;
    }
}
