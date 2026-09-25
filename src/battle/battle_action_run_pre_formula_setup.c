#include "fft/battle.h"
#include "fft/battle_ability.h"
#include "fft/battle_move.h"
#include "fft/data.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/map.h"
#include "fft/menu_types.h"
#include "fft/status.h"
#include "psx/types.h"

/* g_current_ability bound a second time. Spelling every field through one
 * symbol lets GCC share a base register between the two access groups below,
 * which changes this function's code. */
extern battle_current_ability_t g_current_ability_view;

struct battle_action_used_weapon_context;
extern void battle_action_store_used_weapon(struct battle_action_used_weapon_context* action);

/* Pre-formula setup: loads the current ability, weapon and item data for one
 * attacker/target pair and runs the formula handler.
 *
 * Returns 1 when the target was already missed before the formula (reflect,
 * Blade Grasp, Arrow Guard) and 0 otherwise, including the reaction and
 * knockback early exits. id holds the weapon, then the thrown item, then the
 * status-infliction index. The fields the target addresses through a base
 * register, plus ability_id, go through g_current_ability_view; the others
 * use g_current_ability. */
s32 battle_action_run_pre_formula_setup(const u8* source, u8 target_id) {
    battle_ai_command_action_t action;
    weapon_data_t* weapon;
    ability_secondary_data_t* ability;
    item_secondary_data_t* item;
    ability_data_t* data;
    u8 menu_type;
    u8 skillset;
    u8 formula;
    u8 id;
    u16 ability_id;

    main_util_copy_action_data(source, (u8*)&action);
    g_current_ability.target_id = target_id;
    g_battle_action_target = &g_battle_unit_stats[target_id];
    g_battle_action_target_data = &g_battle_unit_stats[target_id].action;
    g_current_ability.post_formula_flag = 0;
    g_current_ability_view.attacker_id = action.unit_id;
    g_battle_action_attacker_data = &g_current_action_data;
    g_battle_action_attacker = &g_battle_unit_stats[g_current_ability_view.attacker_id];
    if (g_battle_action_context != BATTLE_ACTION_CONTEXT_PRIMARY && battle_action_perform_reaction_ability() != 0) {
        battle_action_store_used_weapon((struct battle_action_used_weapon_context*)&action);
        return 0;
    }
    if (g_current_ability.knockback_flags & 0x80) {
        battle_move_apply_knockback();
        return 0;
    }
    g_current_ability_view.terrain
        = g_battle_map_tile_data[battle_map_calculate_location(g_battle_action_target)].surface.bits.type;
    if (g_current_ability.strike_counter == 0) {
        id = g_current_ability.primary_weapon_id;
    } else {
        id = g_current_ability.secondary_weapon_id;
    }
    if (action.skillset == SKILLSET_ID_DRAW_OUT) {
        id = action.item_id;
    }
    if (id >= 0x80) {
        id = ITEM_ID_NOTHING;
    }
    g_current_ability_view.weapon_id = id;
    g_current_ability_view.ability_id = action.ability_id;
    ability_id = action.ability_id;
    g_current_ability.two_hands_active = 0;
    if (g_current_ability_view.ability_id >= 0x171) {
        ability_id = 0;
    }
    weapon = &g_main_item_weapon_data[id];
    main_util_copy_byte_data(
        &g_main_ability_range_data[ability_id], &g_current_ability_view.range_data, sizeof(ability_secondary_data_t));
    main_util_copy_byte_data(weapon, &g_current_ability_view.weapon_data, sizeof(weapon_data_t));
    g_current_ability.proc_id = weapon->proc_id;
    g_current_ability.charge_power = 0;
    g_current_ability.formula = 1;
    g_current_ability.attacker_faith = g_battle_action_attacker->faith;
    g_current_ability.target_faith = g_battle_action_target->faith;
    battle_action_clear_data();
    skillset = action.skillset;
    g_current_ability.skillset = skillset;
    menu_type = g_main_action_menu_types_by_skillset[skillset];
    if (g_current_ability_view.elemental_flags != 0) {
        action.ability_id = g_current_ability_view.reaction_id;
        g_current_ability_view.ability_id = action.ability_id;
        menu_type = ACTION_MENU_TYPE_DEFAULT;
    } else {
        g_current_ability_view.reaction_id = 0;
    }
    data = &g_main_ability_data[(s16)action.ability_id];
    g_battle_action_target_data->status_change = data->ai_flags.bytes[0] & 3;
    if (data->ai_flags.bytes[3] & 1) {
        g_battle_action_target_data->status_change += 0x80;
    }
    if (menu_type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        action.ability_id = action.item_id + 0x26;
    }
    switch (menu_type) {
    case ACTION_MENU_TYPE_ELEMENTS:
        action.ability_id = battle_action_get_elemental_ability_id(g_battle_action_attacker);
        /* fall through */
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_ARITHMETICKS:
    case ACTION_MENU_TYPE_MONSTER:
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        formula = 1;
        if ((s16)action.ability_id < 0x171) {
            if ((g_battle_action_attacker->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FROG)]
                    & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG))
                && g_current_ability_view.elemental_flags == 0) {
                g_current_ability_view.weapon_id = ITEM_ID_NOTHING;
                main_util_copy_byte_data(
                    (u8*)g_main_item_weapon_data, (u8*)&g_current_ability_view.weapon_data, sizeof(weapon_data_t));
            }
            ability = &g_main_ability_range_data[(s16)action.ability_id];
            main_util_copy_byte_data(ability, &g_current_ability_view.range_data, sizeof(ability_secondary_data_t));
            g_current_ability.proc_id = ability->status_infliction_id;
            formula = ability->formula;
            if (!(g_current_ability.range_data.flags_1
                    & (ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE | ABILITY_SECONDARY_FLAG_1_WEAPON_STRIKE))
                && menu_type != ACTION_MENU_TYPE_KATANA_INVENTORY) {
                g_current_ability_view.weapon_id = ITEM_ID_NOTHING;
            }
        }
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        formula = 1;
        id = action.item_id;
        if (id >= 0xF0) {
            g_current_ability_view.weapon_id = id;
            id += 0x10;
            item = &g_main_item_secondary_data[id];
            g_current_ability.used_item_id = id;
            g_current_ability.proc_id = item->status_infliction_id;
            main_util_clear_byte_data(&g_current_ability_view.range_data, sizeof(ability_secondary_data_t));
            formula = item->formula;
        }
        break;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        formula = 0x63;
        break;
    case ACTION_MENU_TYPE_JUMP:
        formula = 0x64;
        break;
    case ACTION_MENU_TYPE_CHARGE:
        ability_id = action.ability_id;
        if ((u16)(ability_id - ABILITY_ID_CHARGE_FIRST) >= 8) {
            ability_id = ABILITY_ID_CHARGE_FIRST;
        }
        g_current_ability.charge_power = g_main_jump_charge_ability_data_by_ability_id[ability_id * 2 + 1];
        /* fall through */
    case ACTION_MENU_TYPE_ATTACK:
        formula = weapon->formula;
        if ((g_current_ability.weapon_data.flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)
            && (g_battle_action_attacker->support_abilities[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS)) {
            if (g_battle_action_attacker->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] == ITEM_ID_NONE
                && g_battle_action_attacker->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] == ITEM_ID_NONE) {
                g_current_ability.two_hands_active = 1;
            }
            if (g_battle_action_attacker->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] == ITEM_ID_NONE
                && g_battle_action_attacker->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] == ITEM_ID_NONE) {
                g_current_ability.two_hands_active = 1;
            }
        }
        break;
    default:
        formula = 1;
        break;
    }
    g_current_ability_view.elemental_flags = 0;
    g_current_ability.knockback_flags = 0;
    if (formula == 0 || formula >= 0x65) {
        formula = 1;
    }
    id = g_current_ability_view.proc_id;
    if (formula == 3) {
        id = 0;
    }
    main_util_copy_byte_data(&g_main_status_infliction_data[id], &g_current_ability_view.status_infliction,
        sizeof(status_infliction_data_t));
    battle_status_remove();
    g_current_ability.formula = formula;
    battle_action_check_reflect_blade_grasp_and_arrow_guard();
    if (g_battle_action_target_data->hit == 0) {
        return 1;
    }
    g_battle_formula_handlers[formula]();
    if (formula < 7) {
        battle_formula_apply_poach_and_train();
    }
    if (formula == 0x64) {
        g_current_ability_view.weapon_id = ITEM_ID_NOTHING;
    }
    battle_action_finalize_target_current_action();
    if (g_battle_action_target_data->special_effect != 0) {
        g_battle_action_target_data->attack_type |= 1;
    }
    return 0;
}
