#include "fft/battle.h"
#include "psx/types.h"

/* Collects the current target's physical evade sources for the ability being
 * resolved: class evade, accessory evade and one value per hand. A hand
 * contributes its weapon's evade when Weapon Guard is active and the unit can
 * still evade, otherwise its shield's evade. Monsters contribute nothing past
 * the class value.
 *
 * Two spellings in the left-hand block are both needed for one local-allocation
 * tie that otherwise puts the weapon record's secondary_data_id in $a2 and the
 * shield record's in $a0, the reverse of the target:
 *
 *  - staging the left weapon's type_flags through `weapon_secondary` before
 *    copying it into `weapon_flags` makes that load reuse the pseudo
 *    local-alloc ties to $a0, which swaps the two secondary ids' registers;
 *  - reading the left shield's flags through an `item_data_t*` gives the
 *    shield record its own address pseudo instead of folding the address into
 *    the load, which keeps the swap from being undone.
 *
 * Spelling the weapon half as a record pointer as well loses three
 * instructions. */
void battle_formula_store_physical_evade_values(void) {
    s32 weapon_guard;
    u8 weapon_flags;
    u8 shield_flags;
    u8 shield_secondary;
    item_data_t* shield_record;
    u8 weapon_secondary;

    weapon_guard = 0;
    if ((g_battle_action_target->reaction_abilities[3] & BATTLE_REACTION_SET_4_WEAPON_GUARD) != 0) {
        weapon_guard = battle_formula_can_unit_evade(g_battle_action_target) == 0;
    }
    g_current_ability.base_hit = 100;
    g_current_ability.accessory_evade = 0;
    g_current_ability.right_shield_evade = 0;
    g_current_ability.left_shield_evade = 0;
    g_current_ability.class_evade
        = g_battle_action_target->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    if ((g_battle_action_target->unit_flags & UNIT_FLAG_MONSTER) != 0) {
        return;
    }

    weapon_flags
        = g_main_item_primary_data[g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]].type_flags;
    if ((weapon_flags & ITEM_TYPE_FLAG_ACCESSORY) != 0) {
        g_current_ability.accessory_evade
            = g_main_item_accessory_data
                  [g_main_item_primary_data[g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]]
                          .secondary_data_id]
                      .physical_evade;
    }

    weapon_flags
        = g_main_item_primary_data[g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]].type_flags;
    weapon_secondary
        = g_main_item_primary_data[g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]]
              .secondary_data_id;
    shield_flags
        = g_main_item_primary_data[g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]].type_flags;
    shield_secondary
        = g_main_item_primary_data[g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]]
              .secondary_data_id;
    if ((weapon_flags & ITEM_TYPE_FLAG_WEAPON) != 0 && weapon_guard != 0) {
        g_current_ability.right_shield_evade = g_main_item_weapon_data[weapon_secondary].evade;
    } else if ((shield_flags & ITEM_TYPE_FLAG_SHIELD) != 0) {
        g_current_ability.right_shield_evade = g_main_item_shield_data[shield_secondary].physical_evade;
    }

    weapon_secondary
        = g_main_item_primary_data[g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]].type_flags;
    weapon_flags = weapon_secondary;
    weapon_secondary = g_main_item_primary_data[g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]]
                           .secondary_data_id;
    shield_record = &g_main_item_primary_data[g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]];
    shield_flags = shield_record->type_flags;
    shield_secondary = g_main_item_primary_data[g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]]
                           .secondary_data_id;
    if ((weapon_flags & ITEM_TYPE_FLAG_WEAPON) != 0 && weapon_guard != 0) {
        g_current_ability.left_shield_evade = g_main_item_weapon_data[weapon_secondary].evade;
    } else if ((shield_flags & ITEM_TYPE_FLAG_SHIELD) != 0) {
        g_current_ability.left_shield_evade = g_main_item_shield_data[shield_secondary].physical_evade;
    }
}
