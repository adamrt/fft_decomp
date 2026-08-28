#include "fft/data.h"
#include "fft/job.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"

/*
 * Check action restrictions and consume MP outside preview mode.
 *
 * Return 0 when allowed, or a rejection code. Only ordinary-menu,
 * non-Mime abilities consume MP; the effective cost is recorded first.
 * Preview skips only subtraction; earlier bookkeeping still runs.
 */
s32 battle_action_check_and_consume_mp(battle_stats_t* unit) {
    u16 ability_id;
    u8 menu;
    s32 result;
    u32 cost;
    s16 mp;
    s16 signed_ability;
    ability_secondary_data_t* ability;

    ability_id = unit->last_ability_id;
    menu = g_main_action_menu_types_by_skillset[unit->last_skillset_id];
    g_current_ability.mp_cost = 0;
    if (g_battle_action_context == BATTLE_ACTION_CONTEXT_PRIMARY)
        main_status_set_action_state(unit, 0xff);
    result = battle_formula_can_unit_evade(unit);
    if (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_DONT_ACT)]
        & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DONT_ACT))
        result = 1;
    if (result)
        return result + 6;
    if (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FROG)]
        & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG)) {
        signed_ability = ability_id;
        if (signed_ability != 0 && signed_ability != ABILITY_ID_FROG_ATTACK
            && signed_ability != ABILITY_ID_BLACK_MAGIC_FROG)
            return 5;
        if (menu == ACTION_MENU_TYPE_CHARGE)
            return 5;
    }
    if (ability_id < ABILITY_ID_ITEM_FIRST) {
        ability = &g_main_ability_range_data[(s16)ability_id];
        if ((menu == ACTION_MENU_TYPE_DEFAULT || menu == ACTION_MENU_TYPE_MONSTER)
            && (ability->flags_3 & ABILITY_SECONDARY_FLAG_3_AFFECTED_BY_SILENCE)
            && (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_SILENCE)]
                & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SILENCE)))
            return 2;
        if (menu == ACTION_MENU_TYPE_DEFAULT && unit->job_id != JOB_ID_MIME) {
            cost = ability->mp_cost;
            if (unit->support_abilities[1] & BATTLE_SUPPORT_SET_2_HALF_MP)
                cost >>= 1;
            g_current_ability.mp_cost = cost;
            /* A signed member view retains the target store-before-load order;
             * casting the unsigned value hoists the MP load and loses a nop. */
            mp = *(s16*)&unit->mp;
            if (mp < (s32)cost)
                return 3;
            if (g_battle_action_state != BATTLE_ACTION_STATE_PREVIEW)
                unit->mp = mp - cost;
        }
    }
    return 0;
}
