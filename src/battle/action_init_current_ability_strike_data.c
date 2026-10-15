#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Seed the per-action "strike" state from the acting unit before an attack
 * resolves: reset the strike counter, elemental and knockback scratch, clear
 * the experience/level-up flags when no action context is live, latch the
 * unit's two weapon slots, and decide how many strikes the action gets
 * (g_action_current_ability.strike_count). Two-swords and the 0x20 ability flag are what allow a second
 * strike; menu type 2 (item) only records the consumed item.
 *
 * The `ability_id >= 0 || ability_id < ABILITY_ID_ITEM_FIRST` guard is the target's own: the
 * second arm is unreachable, so the table read is never actually skipped.
 *
 * `left_hand` is load-bearing: the target issues the equipment[5] reload
 * BEFORE the 0xff store, which costs it the menu-type zero-extension in $v1
 * and makes it re-emit `andi v1,a2,0xff` after the block. Reading the slot
 * straight into the global instead sinks the load past the store and drops
 * that instruction.
 */
void action_init_current_ability_strike_data(unit_stats_t* unit) {
    ability_secondary_data_t* data;
    u8 menu_type;
    u8 weapon_kind;
    u8 chance;
    s16 ability_id;
    u8 support;
    u8 left_hand;

    g_action_current_ability.strike_count = 1;
    g_action_current_ability.strike_counter = 0;
    g_action_current_ability.weapon_proc_pending = 0;
    g_action_current_ability.knockback_flags = 0;
    weapon_kind = 0;
    if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_action_current_ability.can_earn_exp_jp = 0;
        g_action_current_ability.earned_experience = 0;
        g_action_current_ability.level_gained_flag = 0;
        g_action_current_ability.job_level_gained_flag = 0;
    }

    menu_type = g_main_action_menu_types_by_skillset[unit->action_command.skillset_id];
    g_battle_loaded_ability_flags_1 = 0;
    g_action_current_ability.primary_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    g_action_current_ability.secondary_weapon_id = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
    if (g_action_current_ability.primary_weapon_id == ITEM_ID_NONE) {
        left_hand = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
        g_action_current_ability.secondary_weapon_id = ITEM_ID_NONE;
        g_action_current_ability.primary_weapon_id = left_hand;
    }

    if (menu_type == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
        g_action_current_ability.primary_weapon_id = unit->action_command.item_or_equipment;
        return;
    }

    if (menu_type == ACTION_MENU_TYPE_DEFAULT || menu_type == ACTION_MENU_TYPE_MONSTER) {
        ability_id = unit->action_command.ability_id;
        if (ability_id >= 0 || ability_id < ABILITY_ID_ITEM_FIRST) {
            data = &g_main_ability_range_data[ability_id];
            g_battle_loaded_ability_flags_1 = data->flags_1;
            chance = data->x;
            weapon_kind = data->formula;
        }
        if (weapon_kind == ABILITY_FORMULA_MULTIHIT_TRUTH_MAGIC
            || weapon_kind == ABILITY_FORMULA_UNFAITH_MAGICAL_DAMAGE) {
            if (g_action_state == ACTION_STATE_EXECUTE) {
                g_action_current_ability.strike_count = ((chance * rand()) / 32768) + 1;
                return;
            }
        }
        if (weapon_kind == ABILITY_FORMULA_DAMAGE_MA_PLUS_Y_HALF_TIMES_MA) {
            g_action_current_ability.strike_count = chance + 1;
            return;
        }
    }

    if (menu_type == ACTION_MENU_TYPE_JUMP) {
        return;
    }
    if (menu_type != ACTION_MENU_TYPE_ATTACK && menu_type != ACTION_MENU_TYPE_CHARGE) {
        if ((g_battle_loaded_ability_flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE) == 0) {
            return;
        }
    }

    support = unit->ability_flags.support[2];
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG))
        || (unit->unit_flags & UNIT_FLAG_MONSTER)) {
        g_action_current_ability.primary_weapon_id = ITEM_ID_NONE;
        g_action_current_ability.secondary_weapon_id = ITEM_ID_NONE;
    }

    if ((g_action_current_ability.primary_weapon_id == ITEM_ID_NONE
            && g_action_current_ability.secondary_weapon_id == ITEM_ID_NONE)
        || (g_action_current_ability.primary_weapon_id != ITEM_ID_NONE
            && g_action_current_ability.secondary_weapon_id != ITEM_ID_NONE)) {
        if (support & 1) {
            g_action_current_ability.strike_count = 2;
        }
    }
}
