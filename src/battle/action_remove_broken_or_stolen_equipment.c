#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/types.h"

s32 action_remove_broken_or_stolen_equipment(void) {
    s32 i;
    s32 count;
    u8 item;
    unit_stats_t* unit;
    action_data_t* action;

    count = 0;
    if (g_action_target_data->special_effect
        & (ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | ACTION_SPECIAL_EFFECT_STEAL_ITEM)) {
        for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
            if (g_action_target_data->equipment_destroyed & (ACTION_EQUIPMENT_SLOT_HEAD >> i)) {
                item = g_action_target->equipment[i];
                if (item != ITEM_ID_NOTHING && item != ITEM_ID_NONE
                    && !(g_main_item_primary_data[item].type_flags & ITEM_TYPE_FLAG_UNREMOVABLE)) {
                    g_action_target->equipment[i] = ITEM_ID_NONE;
                    count++;
                    if (g_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                        action_increment_item_quantity_for_steal_break(g_action_attacker, item);
                    }
                }
            }
        }
        if (count != 0) {
            if (g_action_state == ACTION_STATE_EXECUTE) {
                main_unit_refresh_stats_and_statuses(g_action_target);
            }
            action = g_action_target_data;
            if (action->equipment_destroyed == ACTION_EQUIPMENT_SLOT_RIGHT_WEAPON
                || action->equipment_destroyed == ACTION_EQUIPMENT_SLOT_LEFT_WEAPON) {
                unit = g_action_target;
                if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                        & STATUS_BYTE_MASK(STATUS_ID_CHARGING))
                    && unit->charged_ability_ct != 0xFF && unit->action_command.skillset_id == SKILLSET_ID_CHARGE) {
                    action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                        |= STATUS_BYTE_MASK(STATUS_ID_CHARGING);
                }
            }
            return 1;
        }
        return 0;
    }
    return 0;
}
