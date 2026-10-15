#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Check action restrictions and consume MP outside preview mode.
 *
 * Return 0 when allowed, or a rejection code. Only ordinary-menu,
 * non-Mime abilities consume MP; the effective cost is recorded first.
 * Preview skips only subtraction; earlier bookkeeping still runs.
 */
s32 action_check_and_consume_mp(unit_stats_t* unit) {
    s16 ability_id;
    u8 menu;
    s32 result;
    u8 cost;
    s16 mp;
    ability_secondary_data_t* ability;

    ability_id = unit->action_command.ability_id;
    menu = g_main_action_menu_types_by_skillset[unit->action_command.skillset_id];
    g_action_current_ability.mp_cost = 0;
    if (g_action_context == ACTION_CONTEXT_PRIMARY)
        main_status_set_action_state(unit, MAIN_UNIT_ACTION_STATE_KEEP_PERFORMING);
    result = battle_unit_get_action_restriction(unit);
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_ACT)] & STATUS_BYTE_MASK(STATUS_ID_DONT_ACT))
        result = BATTLE_UNIT_ACTION_RESTRICTION_CANNOT_ACT;
    if (result)
        return result + 6;
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        if (ability_id != 0 && ability_id != ABILITY_ID_FROG_ATTACK && ability_id != ABILITY_ID_BLACK_MAGIC_FROG)
            return 5;
        if (menu == ACTION_MENU_TYPE_CHARGE)
            return 5;
    }
    if (ability_id >= 0 && ability_id < ABILITY_ID_ITEM_FIRST) {
        ability = &g_main_ability_range_data[ability_id];
        if ((menu == ACTION_MENU_TYPE_DEFAULT || menu == ACTION_MENU_TYPE_MONSTER)
            && (ability->flags_3 & ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE)
            && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)] & STATUS_BYTE_MASK(STATUS_ID_SILENCE)))
            return 2;
        if (menu == ACTION_MENU_TYPE_DEFAULT && unit->job_id != JOB_ID_MIME) {
            cost = ability->mp_cost;
            if (unit->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_HALF_MP)
                cost >>= 1;
            g_action_current_ability.mp_cost = cost;
            /* A signed member view retains the target store-before-load order;
             * casting the unsigned value hoists the MP load and loses a nop. */
            mp = unit->mp;
            if (mp < cost)
                return 3;
            if (g_action_state != ACTION_STATE_PREVIEW)
                unit->mp = mp - cost;
        }
    }
    return 0;
}
