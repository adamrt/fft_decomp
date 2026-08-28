#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/world.h"

/*
 * Checks whether an item may go into a formation unit's equipment slot.
 *
 * Hand slots (0 and 1) also consider the item in the opposite hand: two
 * weapons need both to be Two Swords compatible and the unit to have Two
 * Swords; a shield cannot pair with another shield or a forced-two-hands
 * weapon.
 *
 * The s16 id and its new_item copy reproduce the target's two callee-saved
 * copies of the masked id; an s32 id folds them into one register.
 */
s32 world_formation_can_equip_item_in_slot(s16 unit_index, s16 slot, s32 item_id) {
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

    result = world_formation_lock_equipment_and_abilities(unit_index);
    if (result < 0) {
        return result;
    }
    id = item_id & WORLD_ITEM_ID_MASK;
    new_item = id;
    if (id == ITEM_ID_NOTHING) {
        return 1;
    }
    result = world_unit_can_equip_item_id(unit_index, id);
    if (slot < WORLD_EQUIPMENT_SLOT_HEAD) {
        if (result != 1) {
            return result;
        }
        is_nonstandard_weapon = id >= ITEM_ID_THROWABLE_FIRST;
        forced_two_hands = 0;
        if (is_nonstandard_weapon) {
            two_swords = 0;
        } else {
            flags = g_main_item_weapon_data[id].flags;
            forced_two_hands = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
            two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        }
        other_slot = slot == WORLD_EQUIPMENT_SLOT_RIGHT_HAND;
        other = g_world_formation_unit_pointers[unit_index]->equipment[other_slot] & WORLD_ITEM_ID_MASK;
        other_is_nonstandard_weapon = other >= ITEM_ID_THROWABLE_FIRST;
        other_forced_two_hands = 0;
        if (other_is_nonstandard_weapon) {
            other_two_swords = 0;
        } else {
            flags = g_main_item_weapon_data[other].flags;
            other_forced_two_hands = flags & WEAPON_FLAG_FORCED_TWO_HANDS;
            other_two_swords = flags & WEAPON_FLAG_TWO_SWORDS_COMPATIBLE;
        }

        if (two_swords && other_two_swords && world_ability_has_two_swords(unit_index)) {
            return 1;
        }
        if (!is_nonstandard_weapon && !other_is_nonstandard_weapon && new_item != ITEM_ID_NOTHING
            && other != ITEM_ID_NOTHING) {
            return 0;
        }
        if (is_nonstandard_weapon && other_is_nonstandard_weapon) {
            return 0;
        }
        if (is_nonstandard_weapon && other_forced_two_hands) {
            return 0;
        }
        if (other_is_nonstandard_weapon && forced_two_hands) {
            return 0;
        }
    }
    return result;
}
