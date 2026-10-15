#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

/* Add a stolen or broken-off item to the player inventory while executing,
 * or its price / 4 in gil once the stock is already at 99. */
s32 action_increment_item_quantity_for_steal_break(unit_stats_t* unit, u8 item) {
    u8 item_id;

    if (g_action_state != ACTION_STATE_EXECUTE) {
        return 0;
    }
    if ((unit->initial_team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE) {
        return 0;
    }
    item_id = item & 0xFF;
    if (item_id == ITEM_ID_NOTHING) {
        return 0;
    }
    if (item_id == ITEM_ID_NONE) {
        return 0;
    }
    if (main_item_get_total_equipment_quantity(item_id, 1) >= 0x63) {
        return action_add_war_funds(unit, 0, item_id);
    }
    g_main_item_quantities[item_id] += 1;
    return 0;
}

/* Add total to the war funds, plus a quarter of the item's price when an item
 * is given (action_increment_item_quantity_for_steal_break passes one
 * the stock is full of). Only for a unit on team 0 and only when executing;
 * the funds stay within 0..99,999,999. Returns the amount added. */
s32 action_add_war_funds(unit_stats_t* unit, s32 total, u8 item_id) {
    s32 value;

    if (g_action_state != ACTION_STATE_EXECUTE) {
        return 0;
    }
    if (unit->team_flags & BATTLE_TEAM_MASK) {
        return 0;
    }
    if (item_id != ITEM_ID_NOTHING) {
        total += g_main_item_primary_data[item_id].price >> 2;
    }
    value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + total;
    if (value > 99999999) {
        value = 99999999;
    } else if (value < 0) {
        value = 0;
    }
    script_set_variable(EVENT_SCRIPT_VAR_WAR_FUNDS, value);
    return total;
}

/* Apply an action result's EXP change when executing. Without bit 0x80 the
 * amount is gained: EXP saturates at 255 and the level-up check runs. With it
 * the low seven bits are lost, down to 0. */
void action_handle_steal_exp(unit_stats_t* unit, u8 amount) {
    s32 total;
    u8 capped;
    s16 reduced;

    if (g_action_state != ACTION_STATE_EXECUTE) {
        return;
    }
    if ((amount & 0x80) == 0) {
        total = unit->experience + amount;
        capped = total;
        if (total >= 0x100) {
            capped = 0xFF;
        }
        unit->experience = capped;
        if (main_unit_check_level_up(unit) != 0) {
            if (unit->battle_id == g_battle_acting_unit_id) {
                g_action_current_ability.level_gained_flag = 1;
            }
        }
    } else {
        reduced = unit->experience - (amount & 0x7F);
        if (reduced < 0) {
            reduced = 0;
        }
        unit->experience = reduced;
    }
}

/* Resolve a unit's status changes with the action state set to preview (2). */
void status_resolve_unit_changes_in_preview(s32 unit_id, s32 removal_only) {
    g_action_state = ACTION_STATE_PREVIEW;
    status_resolve_unit_changes(unit_id, removal_only);
    g_action_state = ACTION_STATE_EXECUTE;
}

/*
 * Remove the statuses the unit's action marks for removal.
 *
 * Only statuses the unit currently has are kept in the removal mask; each one
 * gets its CT cleared and its special flags dropped, then the bits are cleared
 * from both the current and innate status sets.
 */
void status_apply_unit_action_removal(unit_stats_t* unit) {
    s32 i;
    s32 battle_id;
    s32 idx;
    s32 mask;

    g_action_target_data = &unit->action;
    battle_id = unit->battle_id;
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        g_action_target_data->status_changes.removal[i] &= unit->status_sets.current[i];
    }
    for (i = 0; i < STATUS_COUNT; i++) {
        idx = i / 8;
        mask = 0x80 >> (i & 7);
        if (g_action_target_data->status_changes.removal[idx] & mask) {
            main_status_set_ct(unit, i, 0);
            status_enable_special_flags(STATUS_HANDLER_INDEX(i), 0, battle_id);
        }
    }
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        unit->status_sets.current[i] &= ~g_action_target_data->status_changes.removal[i];
        unit->status_sets.innate[i] &= ~g_action_target_data->status_changes.removal[i];
    }
}
