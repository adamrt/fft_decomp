#include "fft/event_equip.h"

/* Validate a candidate item against slot locks, job rules, and hand pairing.
 *
 * The s16 slot parameter is retained because it reproduces the original
 * calling convention; equip_unit_equipment_slot_e names its accepted values.
 * The s16 id and its new_item copy reproduce the target's two callee-saved
 * copies of the masked id (the same shape as
 * world_formation_can_equip_item_in_slot). */
equip_item_slot_result_e equip_unit_validate_slot_item(s16 unit, s16 slot, s32 item_id) {
    s32 result;
    s16 id;
    s16 new_item;
    s32 other;
    s32 other_slot;
    s32 is_nonstandard_weapon;
    s32 other_is_nonstandard_weapon;
    s32 forced_two_hands;
    s32 other_forced_two_hands;
    s32 two_swords;
    s32 other_two_swords;
    u8 flags;

    id = item_id & EQUIP_ITEM_ID_MASK;
    new_item = id;
    if (id == ITEM_ID_NOTHING) {
        return EQUIP_SLOT_ITEM_ALLOWED;
    }
    if (g_main_item_primary_data[g_equip_unit_data[unit]->equipment[slot]].type_flags & ITEM_TYPE_FLAG_UNREMOVABLE) {
        return EQUIP_SLOT_ITEM_CURRENT_LOCKED;
    }
    result = equip_unit_can_equip_item(unit, id);
    if (slot < EQUIP_SLOT_HAND_COUNT && result == EQUIP_SLOT_ITEM_ALLOWED) {
        is_nonstandard_weapon = id >= ITEM_ID_THROWABLE_FIRST;
        if (is_nonstandard_weapon) {
            forced_two_hands = 0;
            two_swords = 0;
        } else {
            flags = g_main_item_weapon_data[id].flags;
            forced_two_hands = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
            two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        }
        other_slot = slot == EQUIP_SLOT_RIGHT_HAND;
        other = g_equip_unit_data[unit]->equipment[other_slot] & EQUIP_ITEM_ID_MASK;
        other_is_nonstandard_weapon = other >= ITEM_ID_THROWABLE_FIRST;
        if (other_is_nonstandard_weapon) {
            other_forced_two_hands = 0;
            other_two_swords = 0;
        } else {
            flags = g_main_item_weapon_data[other].flags;
            other_forced_two_hands = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
            other_two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        }

        if (two_swords && other_two_swords && equip_unit_has_two_swords(unit)) {
            return EQUIP_SLOT_ITEM_ALLOWED;
        }
        if (!is_nonstandard_weapon && !other_is_nonstandard_weapon && new_item != ITEM_ID_NOTHING
            && other != ITEM_ID_NOTHING) {
            return EQUIP_SLOT_ITEM_INCOMPATIBLE_PAIR;
        }
        if (is_nonstandard_weapon && other_is_nonstandard_weapon) {
            return EQUIP_SLOT_ITEM_INCOMPATIBLE_PAIR;
        }
        if (is_nonstandard_weapon && other_forced_two_hands) {
            return EQUIP_SLOT_ITEM_INCOMPATIBLE_PAIR;
        }
        if (other_is_nonstandard_weapon && forced_two_hands) {
            return EQUIP_SLOT_ITEM_INCOMPATIBLE_PAIR;
        }
    }
    return result;
}
