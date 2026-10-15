#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Give the single unit targeted by `id`'s pending action a chance to Hamedo
 * the actor, reporting the reaction ability through `out_ability`.
 *
 * Returns the reacting unit's slot, or -1 when nothing reacts.
 *
 * target_validate_weapon_target takes only the action record (it copies the record and
 * rederives the menu type itself); menu_type and flags_1 merely happen to sit
 * in $a1/$a2 at that call. Passing them as arguments would set $a2 twice and
 * change the scheduling of the counter-ability call's argument setup.
 */
s32 battle_reaction_prepare_hamedo_for_pending_action(s32 id, s16* out_ability) {
    u8 targets[BATTLE_UNIT_SLOT_COUNT];
    unit_stats_t* unit;
    unit_stats_t* base;
    unit_stats_t* target;
    action_command_t* action;
    s32 i;
    s32 count;
    s32 result;
    s32 menu_type;
    u8 flags_1;
    u16 mp;
    s32 tid;
    u16 ability_id;

    unit = &g_battle_unit_stats[id];
    base = g_battle_unit_stats;
    g_action_hamedo_flag = 0;
    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    mp = unit->mp;
    result = action_check_and_consume_mp(unit);
    unit->mp = mp;
    if (result != 0) {
        return -1;
    }

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        base[i].action.reaction_id = 0;
        base[i].ability_outcome = 0;
    }

    menu_type = g_main_action_menu_types_by_skillset[unit->action_command.skillset_id];
    flags_1 = 0;
    if ((menu_type & 0xff) == ACTION_MENU_TYPE_DEFAULT || (menu_type & 0xff) == ACTION_MENU_TYPE_MONSTER) {
        ability_id = unit->action_command.ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            flags_1 = g_main_ability_range_data[ability_id].flags_1;
        }
    }

    result = -1;
    if ((menu_type & 0xff) == ACTION_MENU_TYPE_ATTACK || (menu_type & 0xff) == ACTION_MENU_TYPE_CHARGE
        || (menu_type & 0xff) == ACTION_MENU_TYPE_WEAPON_INVENTORY
        || (flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
        result = target_validate_weapon_target(&unit->action_command);
    }
    if (result == -1) {
        return -1;
    }

    g_battle_sort_targets_nearest_first = 0;
    count = target_list_units_on_panels(targets, unit);
    if (count != 1) {
        return -1;
    }
    tid = targets[0];
    if (tid == id) {
        return -1;
    }
    target = &g_battle_unit_stats[tid];
    if (!(target->ability_flags.reaction[3] & BATTLE_REACTION_SET_4_HAMEDO)) {
        return -1;
    }
    if (action_calculate_chance_to_react(target) != 0) {
        return -1;
    }
    action = &target->action_command;
    if (action_is_reaction_prevented(target) != 0) {
        return -1;
    }

    g_action_context = count;
    main_util_copy_action_data(action, &g_action_saved_command);
    g_battle_current_reaction_ability_id = ABILITY_ID_REACTION_HAMEDO;
    g_battle_acting_unit_id = id;
    target->action_command.actor_id = tid;
    if (action_store_counter_ability(target, SKILLSET_ID_ATTACK, 0, 1) == 0
        && action_check_and_consume_mp(target) == 0) {
        action_init_current_ability_strike_data(target);
        *out_ability = g_battle_current_reaction_ability_id;
        g_action_hamedo_flag = count;
        if (unit->job_id == JOB_ID_MIME) {
            unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
            unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
        }
        return tid;
    }
    main_util_copy_action_data(&g_action_saved_command, action);
    return -1;
}
