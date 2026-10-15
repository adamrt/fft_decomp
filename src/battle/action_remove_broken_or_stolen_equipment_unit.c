#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/types.h"

/* Remove the target's equipment named by the action's destroyed-slot mask.
 *
 * Slots holding no item, or an item marked unremovable, are skipped. A steal
 * adds each removed item to the attacker's inventory. When anything was
 * removed the target's stats are refreshed outside the action preview, and
 * losing either weapon cancels a Charge in progress by removing the charging
 * status. Returns 1 when an item was removed. */
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

/* Apply one stat change of an action result: the low seven bits are the amount
 * and bit 0x80 means increase. The result is clamped to min..max; returns 1
 * when the stat changed. With max 255 (CT) an amount of 0x7f stands for 255,
 * which empties or fills CT. With min 1 (Speed, PA, MA) stat is the base
 * value: the total six bytes on is rebuilt as base + equipment bonus, capped
 * at max.
 *
 * The caller's ranges: Speed 1..50, PA and MA 1..99, Brave 0..100 (10 at least
 * for a rider), Faith 0..100, CT 0..255. */
s32 battle_unit_apply_stat_increment_decrement(s32 mod, u8* stat, u8 max, u8 min) {
    s32 amount;
    s32 value;

    value = stat[0];
    amount = mod & ACTION_STAT_CHANGE_VALUE_MASK;
    if (max == 0xFF && amount == 0x7F) {
        amount = 0xFF;
    }

    if ((mod & ACTION_STAT_CHANGE_INCREASE) != 0) {
        value = value + amount;
    } else {
        value = value - amount;
    }

    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }

    if (value == stat[0]) {
        return 0;
    }
    stat[0] = value;

    if (min == 1) {
        value = stat[0] + stat[3];
        if (value > max) {
            value = max;
        }
        stat[6] = value;
    }
    return 1;
}

/* Record the highest EXP award produced by the current action.
 *
 * Success starts at 10; kills divide 20 by KO count. Level difference and
 * Gained EXP Up then adjust the result, which is clamped to 1 through 99. */
void battle_unit_update_attacker_earned_experience(s32 outcome) {
    s16 base;
    s32 signed_exp;
    s32 divisor;
    unit_stats_t* attacker;
    s16 exp;
    s32 sum;

    if ((g_action_context == ACTION_CONTEXT_PRIMARY) && (outcome != 0)) {
        if (outcome == 1) {
            base = 10;
        } else {
            divisor = g_action_target->ko_count;
            base = 20;
            if (divisor != 0) {
                base = 20 / divisor;
            }
        }
        attacker = g_action_attacker;
        sum = (base + g_action_target->level) - attacker->level;
        exp = sum;
        if (exp <= 0) {
            exp = 1;
        }
        if (attacker->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_GAINED_EXP_UP) {
            exp = exp * 2;
        }
        if (exp >= 100) {
            exp = 99;
        }
        signed_exp = exp << 16;
        if (g_action_current_ability.earned_experience < (signed_exp >> 16)) {
            g_action_current_ability.earned_experience = exp;
        }
    }
}

/* Apply the prepared action result to one target: make the unit the current
 * target, copy the 0x2c-byte result from g_action_attacker_action_data
 * into its action record and finalize it with
 * action_finalize_attack_and_flag_reactions.
 *
 * Returns that routine's result, or -1 for an invalid or empty slot. A result
 * that is not a hit is marked as canceled. */
s32 action_store_target_stats_pointer_data(s32 unit_id) {
    unit_stats_t* stats;
    action_data_t* action;
    s32 result;

    stats = &g_battle_unit_stats[unit_id];
    g_action_target = stats;
    if (unit_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }
    action = &g_battle_unit_stats[unit_id].action;
    g_action_current_ability.target_id = unit_id;
    g_action_target_data = action;
    if (stats->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    mem_copy_byte_data_source_first(&g_action_attacker_action_data, action, 0x2C);
    result = action_finalize_attack_and_flag_reactions(unit_id);
    if (g_action_target_data->hit == 0) {
        g_action_target_data->miss_type = ACTION_MISS_TYPE_CANCELED;
    }
    return result;
}
