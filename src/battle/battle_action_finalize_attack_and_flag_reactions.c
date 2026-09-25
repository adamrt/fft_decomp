#include "fft/battle.h"
#include "psx/types.h"

#define STATUS_MASK(id) BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_##id)

/* Apply the target's pending action results and flag reactions.
 *
 * Returns -1 for an invalid or absent unit, or when a mounted target ends
 * the action dead or crystallized; otherwise 0.
 *
 * The HP/MP damage and healing fields are read signed (lh), hence the s16
 * casts on the u16 header fields. The KO test's three status-bit checks fold
 * into the target's single halfword load of current[0..1] & 0x160. */
s32 battle_action_finalize_attack_and_flag_reactions(s32 unit_id) {
    s32 outcome;
    s32 hp;
    s32 mp;
    s32 i;
    u16 old_hp;
    u16 old_mp;
    u8 old_status[5];
    u8 old_status_ct[16];

    outcome = 0;
    if (unit_id >= 21) {
        return -1;
    }
    g_current_ability.target_id = unit_id;
    g_battle_action_target = &g_battle_unit_stats[unit_id];
    g_battle_action_target_data = &g_battle_unit_stats[unit_id].action;
    if (g_battle_action_target->entd_slot == BATTLE_ENTD_SLOT_NONE) {
        return -1;
    }
    if ((g_battle_action_target->status_sets.current[0] & STATUS_MASK(DEAD))
        && ((g_battle_action_target_data->status_infliction[0] & STATUS_MASK(CRYSTAL))
            || (g_battle_action_target_data->status_infliction[1] & STATUS_MASK(TREASURE)))) {
        battle_status_resolve_unit_changes(unit_id, 0);
        return 0;
    }
    if (g_battle_action_context == BATTLE_ACTION_CONTEXT_PRIMARY) {
        g_battle_action_target->ability_outcome = 1;
    }
    battle_action_dispatch_target_reaction_ability();
    if ((s16)g_battle_action_target_data->reaction_id == ABILITY_ID_REACTION_CATCH) {
        battle_action_increment_item_quantity_for_steal_break(
            g_battle_action_target, (u8)g_battle_action_target_data->last_received_attack);
    }
    if (g_battle_action_target_data->hit == 0) {
        return 0;
    }
    if (g_battle_action_target_data->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_GOLEM_GUARD) {
        u16* golem;

        golem = &g_battle_team_golem[(g_battle_action_target->initial_team_flags & BATTLE_TEAM_MASK) >> 4];
        hp = *golem - (s16)g_battle_action_target_data->hp_damage;
        if (hp < 0) {
            hp = 0;
        }
        *golem = hp;
        return 0;
    }
    if (g_battle_action_target_data->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_SET_GOLEM) {
        outcome = 1;
        g_battle_team_golem[(g_battle_action_target->team_flags & BATTLE_TEAM_MASK) >> 4]
            = g_battle_action_target->max_hp;
    }
    battle_action_check_mp_switch_distribute_and_damage_split_usability();
    old_hp = g_battle_action_target->hp;
    hp = old_hp - (s16)g_battle_action_target_data->hp_damage + (s16)g_battle_action_target_data->hp_healing;
    if (hp < 0) {
        hp = 0;
    }
    if (g_battle_action_target->max_hp < hp) {
        hp = g_battle_action_target->max_hp;
    }
    old_mp = g_battle_action_target->mp;
    mp = old_mp - (s16)g_battle_action_target_data->mp_damage + (s16)g_battle_action_target_data->mp_healing;
    if (mp < 0) {
        mp = 0;
    }
    if (g_battle_action_target->max_mp < mp) {
        mp = g_battle_action_target->max_mp;
    }
    if (old_hp != hp || old_mp != mp) {
        outcome = 1;
    }
    g_battle_action_target->hp = hp;
    g_battle_action_target->mp = mp;
    outcome |= battle_unit_apply_stat_increment_decrement(
        g_battle_action_target_data->sp_change, &g_battle_action_target->base_attributes[2], 50, 1);
    outcome |= battle_unit_apply_stat_increment_decrement(
        g_battle_action_target_data->ct_change, &g_battle_action_target->ct, 255, 0);
    outcome |= battle_unit_apply_stat_increment_decrement(
        g_battle_action_target_data->pa_change, &g_battle_action_target->base_attributes[0], 99, 1);
    outcome |= battle_unit_apply_stat_increment_decrement(
        g_battle_action_target_data->ma_change, &g_battle_action_target->base_attributes[1], 99, 1);
    outcome |= battle_unit_apply_stat_increment_decrement(g_battle_action_target_data->brave_change,
        &g_battle_action_target->brave, 100, (g_battle_action_target->mount_info & 0x80) ? 10 : 0);
    outcome |= battle_unit_apply_stat_increment_decrement(
        g_battle_action_target_data->faith_change, &g_battle_action_target->faith, 100, 0);
    outcome |= battle_action_remove_broken_or_stolen_equipment();
    if (g_battle_action_target_data->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN) {
        battle_action_increment_item_quantity_for_steal_break(
            g_battle_action_target, (u8)g_battle_action_target->used_item_or_equipment);
    }
    if (g_battle_action_target_data->gil_change != 0 || g_battle_action_target_data->exp_change != 0) {
        outcome |= 1;
    }
    battle_action_add_war_funds(g_battle_action_target, g_battle_action_target_data->gil_change, 0);
    battle_action_handle_steal_exp(g_battle_action_target, g_battle_action_target_data->exp_change);
    if (g_battle_action_target_data->jp_change != 0) {
        battle_action_run_main_reaction_and_flag_job_level_change(g_battle_action_target);
    }
    outcome |= battle_unit_apply_level_up_down_ability();
    outcome |= battle_action_add_poached_item_to_fur_shop_inventory();
    if (g_battle_action_target_data->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_MORBOL) {
        outcome |= 1;
        if (g_battle_action_state == BATTLE_ACTION_STATE_EXECUTE) {
            battle_status_reapply_active_flags(g_battle_action_target);
        }
    }
    if (hp == 0 && !(g_battle_action_target->status_sets.current[0] & STATUS_MASK(CRYSTAL))
        && !(g_battle_action_target->status_sets.current[0] & STATUS_MASK(DEAD))
        && !(g_battle_action_target->status_sets.current[1] & STATUS_MASK(TREASURE))) {
        battle_action_clear_status_changes(g_battle_action_target_data);
        g_battle_action_target_data->status_infliction[0] = STATUS_MASK(DEAD);
        battle_status_resolve_unit_changes(unit_id, 1);
        if (g_battle_action_state == BATTLE_ACTION_STATE_EXECUTE) {
            g_battle_action_target->ko_count++;
        }
        battle_unit_update_attacker_earned_experience(2);
        return -(g_battle_action_target->mount_info >> 7);
    }
    if (hp <= (u16)(g_battle_action_target->max_hp / 5)) {
        g_battle_action_target_data->status_infliction[2] |= STATUS_MASK(CRITICAL);
    } else {
        g_battle_action_target_data->status_removal[2] |= STATUS_MASK(CRITICAL);
    }
    if (g_battle_action_target->brave < 10) {
        g_battle_action_target_data->status_infliction[2] |= STATUS_MASK(CHICKEN);
    } else {
        g_battle_action_target_data->status_removal[2] |= STATUS_MASK(CHICKEN);
    }
    if (g_battle_action_target_data->attack_type & 0x80) {
        g_battle_action_target_data->status_removal[4] |= STATUS_MASK(CHARM) | STATUS_MASK(SLEEP);
        g_battle_action_target_data->status_removal[2] |= STATUS_MASK(TRANSPARENT);
        g_battle_action_target_data->status_removal[1] |= STATUS_MASK(CONFUSION);
    }
    if ((g_battle_action_target_data->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_KNOCKBACK)
        && battle_status_remove_charging_ability_ct(g_battle_action_target, 0) != 0) {
        g_battle_action_target_data->status_removal[0] |= STATUS_MASK(CHARGING);
        g_battle_action_target->charged_ability_ct = 0xff;
    }
    battle_status_modify_inflictions(0);
    for (i = 0; i < 5; i++) {
        old_status[i] = g_battle_action_target->status_sets.current[i];
    }
    for (i = 0; i < 16; i++) {
        old_status_ct[i] = g_battle_action_target->status_ct[i];
    }
    battle_status_resolve_unit_changes(unit_id, 0);
    if ((g_battle_action_target->status_sets.current[0] & STATUS_MASK(DEAD)) && !(old_status[0] & STATUS_MASK(DEAD))) {
        outcome = 2;
        if (g_battle_action_state == BATTLE_ACTION_STATE_EXECUTE) {
            g_battle_action_target->ko_count++;
        }
    } else {
        for (i = 0; i < 5; i++) {
            if (g_battle_action_target->status_sets.current[i] != old_status[i]) {
                outcome = 1;
                break;
            }
        }
        for (i = 0; i < 16; i++) {
            if (g_battle_action_target->status_ct[i] != old_status_ct[i]) {
                outcome = 1;
                break;
            }
        }
    }
    battle_unit_update_attacker_earned_experience(outcome);
    if (outcome != 0) {
        g_battle_action_target->ability_outcome = 2;
    }
    battle_action_check_stat_save_and_restore_reaction_usability();
    if (g_battle_action_target->status_sets.current[0] & (STATUS_MASK(CRYSTAL) | STATUS_MASK(DEAD))) {
        if (g_battle_action_target->mount_info & 0x80) {
            return -1;
        }
    }
    return 0;
}
