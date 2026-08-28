#ifndef FFT_UNIT_EQUIPMENT_H
#define FFT_UNIT_EQUIPMENT_H

/*
 * Seven-slot equipment order serialized by party_data_t and battle_stats_t.
 * This differs from the five-slot right-hand-first order used by menu records.
 */
typedef enum unit_equipment_slot {
    UNIT_EQUIPMENT_SLOT_HEAD = 0,
    UNIT_EQUIPMENT_SLOT_BODY = 1,
    UNIT_EQUIPMENT_SLOT_ACCESSORY = 2,
    UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON = 3,
    UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD = 4,
    UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON = 5,
    UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD = 6,
    UNIT_EQUIPMENT_SLOT_COUNT = 7,
} unit_equipment_slot_e;

#endif
