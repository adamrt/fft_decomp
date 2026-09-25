#include "fft/battle.h"
#include "psx/types.h"

/* Remove the target's equipment named by the action's destroyed-slot mask.
 *
 * Slots holding no item, or an item marked unremovable, are skipped. A steal
 * adds each removed item to the attacker's inventory. When anything was
 * removed the target's stats are refreshed outside the action preview, and
 * losing either weapon cancels a Charge in progress by removing the charging
 * status. Returns 1 when an item was removed. */
s32 battle_action_remove_broken_or_stolen_equipment(void) {
    s32 i;
    s32 count;
    u8 item;
    battle_stats_t* unit;
    battle_action_data_t* action;

    count = 0;
    if (g_battle_action_target_data->special_effect
        & (BATTLE_ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | BATTLE_ACTION_SPECIAL_EFFECT_STEAL_ITEM)) {
        for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
            if (g_battle_action_target_data->equipment_destroyed & (BATTLE_ACTION_EQUIPMENT_SLOT_HEAD >> i)) {
                item = g_battle_action_target->equipment[i];
                if (item != ITEM_ID_NOTHING && item != ITEM_ID_NONE
                    && !(g_main_item_primary_data[item].type_flags & ITEM_TYPE_FLAG_UNREMOVABLE)) {
                    g_battle_action_target->equipment[i] = ITEM_ID_NONE;
                    count++;
                    if (g_battle_action_target_data->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                        battle_action_increment_item_quantity_for_steal_break(g_battle_action_attacker, item);
                    }
                }
            }
        }
        if (count != 0) {
            if (g_battle_action_state == BATTLE_ACTION_STATE_EXECUTE) {
                main_unit_refresh_stats_and_statuses(g_battle_action_target);
            }
            action = g_battle_action_target_data;
            if (action->equipment_destroyed == BATTLE_ACTION_EQUIPMENT_SLOT_RIGHT_WEAPON
                || action->equipment_destroyed == BATTLE_ACTION_EQUIPMENT_SLOT_LEFT_WEAPON) {
                unit = g_battle_action_target;
                if ((unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING))
                    && unit->charged_ability_ct != 0xFF && unit->last_skillset_id == SKILLSET_ID_CHARGE) {
                    action->status_removal[0] |= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING);
                }
            }
            return 1;
        }
        return 0;
    }
    return 0;
}
