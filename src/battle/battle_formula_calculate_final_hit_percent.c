#include "fft/battle.h"
#include "psx/types.h"

/* Scale the action accuracy by each evade source, then roll them in turn.
 *
 * Applies to the basic attack (ability id 0) or abilities flagged evadeable
 * (flags4 bit 0x02). Any evade at or above the base hit forces a miss;
 * otherwise the accuracy becomes the product of the four (hit - evade)
 * percentages. The rolls then run accessory, right hand, left hand and
 * class evade, reporting the item that blocked. */
void battle_formula_calculate_final_hit_percent(void) {
    u8 hit;
    u8 accessory;
    u8 right;
    u8 left;
    u8 evade;
    u8 item;

    if ((g_current_ability.ability_id != 0)
        && !(g_current_ability.range_data.flags_4 & ABILITY_SECONDARY_FLAG_4_EVADEABLE)) {
        return;
    }
    hit = g_current_ability.base_hit;
    accessory = g_current_ability.accessory_evade;
    if ((accessory >= hit) || ((right = g_current_ability.right_shield_evade) >= hit)
        || ((left = g_current_ability.left_shield_evade) >= hit) || ((evade = g_current_ability.class_evade) >= hit)) {
        g_battle_action_target_data->hit = 0;
        g_battle_action_target_data->attack_accuracy = 0;
    } else {
        g_battle_action_target_data->attack_accuracy
            = ((hit - accessory) * 100 / hit * ((hit - right) * 100 / hit) * ((hit - left) * 100 / hit)
                  * ((hit - evade) * 100 / hit) / 1000000)
            * (s16)g_battle_action_target_data->attack_accuracy / 100;
    }
    if (battle_formula_calculate_attack_evaded(
            g_current_ability.accessory_evade, hit, 1, g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY])
        != 0) {
        return;
    }
    item = g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    if (item == ITEM_ID_NONE) {
        item = g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
    }
    if (battle_formula_calculate_attack_evaded(g_current_ability.right_shield_evade, hit, 2, item) != 0) {
        return;
    }
    item = g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
    if (item == ITEM_ID_NONE) {
        item = g_battle_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
    }
    if (battle_formula_calculate_attack_evaded(g_current_ability.left_shield_evade, hit, 3, item) != 0) {
        return;
    }
    battle_formula_calculate_attack_evaded(g_current_ability.class_evade, hit, 4, ITEM_ID_NONE);
}
