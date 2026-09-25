#include "fft/battle.h"
#include "psx/types.h"

/* Reset the base hit to 100 and load the target's accessory and shield
 * magic evades; monsters get no equipment evade. */
void battle_formula_store_magical_evade_values(void) {
    battle_stats_t* target;
    item_data_t* items;
    item_data_t* item;
    s32 secondary;

    target = g_battle_action_target;
    g_current_ability.base_hit = 100;
    g_current_ability.accessory_evade = 0;
    g_current_ability.right_shield_evade = 0;
    g_current_ability.left_shield_evade = 0;
    g_current_ability.class_evade = 0;
    if (target->unit_flags & UNIT_FLAG_MONSTER) {
        return;
    }
    items = g_main_item_primary_data;
    item = &items[target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]];
    if (item->type_flags & ITEM_TYPE_FLAG_ACCESSORY) {
        g_current_ability.accessory_evade = g_main_item_accessory_data[item->secondary_data_id].magic_evade;
    }
    item = &items[target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]];
    secondary = item->secondary_data_id;
    if (item->type_flags & ITEM_TYPE_FLAG_SHIELD) {
        g_current_ability.right_shield_evade = g_main_item_shield_data[secondary].magic_evade;
    }
    item = &items[target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]];
    secondary = item->secondary_data_id;
    if (item->type_flags & ITEM_TYPE_FLAG_SHIELD) {
        g_current_ability.left_shield_evade = g_main_item_shield_data[secondary].magic_evade;
    }
}
