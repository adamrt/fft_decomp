#include "fft/main.h"

/* Map ENTD's five equipment bytes into the seven simulation slots. */
void main_unit_store_monster_equipment(battle_stats_t* unit, const entd_unit_t* entd) {
    unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = entd->equipment[ENTD_EQUIPMENT_SLOT_RIGHT_HAND];
    unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = ITEM_ID_NONE;
    unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = entd->equipment[ENTD_EQUIPMENT_SLOT_LEFT_HAND];
    unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = ITEM_ID_NONE;
    unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD] = entd->equipment[ENTD_EQUIPMENT_SLOT_HEAD];
    unit->equipment[UNIT_EQUIPMENT_SLOT_BODY] = entd->equipment[ENTD_EQUIPMENT_SLOT_BODY];
    unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY] = entd->equipment[ENTD_EQUIPMENT_SLOT_ACCESSORY];
}
