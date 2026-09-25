#include "fft/battle.h"
#include "fft/data.h"
#include "fft/map.h"

struct battle_action_used_weapon_context;

extern void battle_action_store_used_weapon(struct battle_action_used_weapon_context* action);

/* Resolve one strike of the current ability: build the target list, run the
 * per-target formula setup and fill the strike work record.
 *
 * Returns -1 for an invalid actor or targeting, 1 when nothing was hit and 0
 * otherwise. After the clearing loop i holds 21, which the target reuses as
 * the Math skillset id in the control-value test. The (u16*)/(s16*) views
 * reproduce the target's load forms and memory-access ordering. */
s32 battle_action_resolve_ability_strike(s32 misc_unit_id, battle_strike_work_t* work) {
    u8 targets[21];
    s32 count;
    u8 flags_3;
    battle_stats_t* attacker;
    s32 i;
    s32 reaction_targeted;
    u8 target_id;
    u16 reaction_id;
    map_tile_t* tile;

    g_battle_sort_targets_nearest_first = 0;
    g_battle_relocated_unit_count = 0;
    g_current_ability.random_fire_flag = 0;
    for (i = 0; i < 21; i++) {
        targets[i] = 0xff;
    }
    if (misc_unit_id >= 21) {
        return -1;
    }
    attacker = &g_battle_unit_stats[misc_unit_id];
    battle_action_clear_current_data(&attacker->action);
    attacker->action.hit = 0;
    attacker->action.miss_type = BATTLE_ACTION_MISS_TYPE_CANCELLED;
    battle_action_clear_current_data(&g_current_action_data);
    g_current_action_data.hit = 0;
    work->knockback_flags = 0;
    work->ability_formula = 0;
    work->control_value_19f = 0;
    work->reaction_ability_id = 0;
    reaction_targeted = 0;
    if (g_battle_action_context != BATTLE_ACTION_CONTEXT_PRIMARY) {
        work->reaction_ability_id = g_battle_current_reaction_ability_id;
        if (battle_action_build_reaction_targets(misc_unit_id, work, &count, targets)) {
            if (g_battle_current_reaction_ability_id == ABILITY_ID_REACTION_REFLECT) {
                flags_3 = 1;
            } else {
                flags_3 = 0;
            }
            reaction_targeted = 1;
        }
    } else if (attacker->last_skillset_id == i) {
        work->control_value_19f = 1;
    }
    if (!reaction_targeted) {
        if (battle_target_calculate_map_for_action((battle_ai_command_action_t*)&attacker->action_actor_id, &flags_3)
            == -1) {
            work->target_count = 0;
            work->continue_attack = 0;
            return -1;
        }
        if (g_current_ability.post_action_target_id < 21
            && (g_current_ability.elemental_flags != 0 || (g_current_ability.knockback_flags & 0x80))) {
            battle_target_disable_green_panel_flags();
            count = 1;
            targets[0] = g_current_ability.post_action_target_id;
            for (i = 1; i < 16; i++) {
                targets[i] = 0xff;
            }
            tile = &g_battle_map_tile_data[battle_map_calculate_location(
                &g_battle_unit_stats[g_current_ability.post_action_target_id])];
            tile->ceiling_depth_and_marks |= MAP_TILE_FLAG_TARGETED;
        }
        count = battle_target_list_units_on_panels(targets, attacker);
    }
    g_current_ability.target_count = count;
    battle_action_store_ability_data(targets);
    battle_action_store_used_weapon((struct battle_action_used_weapon_context*)&attacker->action_actor_id);
    for (i = 0; i < 21; i++) {
        target_id = targets[i];
        if (target_id != 0xff) {
            g_current_ability.defaulted_to_attack = 0;
            battle_action_run_pre_formula_setup(&attacker->action_actor_id, target_id);
        }
    }
    battle_status_remove_transparent_if_jump_used(attacker);
    count = battle_target_count_hit_by_ability(work->target_list);
    work->target_count = count;
    g_current_ability.target_count = count;
    if (g_battle_action_context == BATTLE_ACTION_CONTEXT_PRIMARY) {
        g_current_ability.can_earn_exp_jp = 1;
    }
    work->can_earn_experience = 0;
    battle_action_finalize_draw_out_katana_result(attacker, work, (u8)count);
    work->actor_id = misc_unit_id;
    work->used_weapon_id = g_current_ability.weapon_id;
    work->reaction_occurred = 0;
    work->current_hit_number = g_current_ability.strike_counter;
    battle_target_set_coordinates_for_ability(attacker, work);
    if (count == 0) {
        g_current_ability.strike_counter++;
        work->last_attack_id = *(u16*)&attacker->last_ability_id;
        work->reaction_id_1a = 0;
        if (!(flags_3 & 1)) {
            work->animate_on_miss_flag = 1;
        }
        if (g_current_ability.strike_counter < g_current_ability.strike_count) {
            work->continue_attack = 1;
        } else {
            work->continue_attack = 0;
        }
        return 1;
    }
    work->ability_formula = g_current_ability.post_formula_flag;
    work->animate_on_miss_flag = 0;
    reaction_id = g_current_ability.reaction_id;
    if (reaction_id != 0 && g_current_ability.elemental_flags == 0) {
        work->last_attack_id = reaction_id;
        work->reaction_occurred = 1;
    } else if (g_current_ability.defaulted_to_attack != 0) {
        work->last_attack_id = 0;
    } else {
        work->last_attack_id = attacker->last_ability_id;
    }
    *(s16*)&work->reaction_id_1a = g_current_ability.reaction_id;
    if (g_current_ability.knockback_flags != 0) {
        work->knockback_flags = g_current_ability.knockback_flags & 0x7f;
        work->target_new_x = g_current_ability.target_x;
        work->target_new_y = g_current_ability.target_y;
        work->target_new_map_level = g_current_ability.target_elevation;
    }
    if (g_current_ability.elemental_flags != 0 || (g_current_ability.knockback_flags & 0x80)) {
        work->continue_attack = 1;
    } else {
        if (g_current_ability.knockback_flags != 0) {
            work->reaction_occurred = 1;
            work->animate_on_miss_flag = 1;
            work->last_attack_id = 0x200;
        }
        g_current_ability.strike_counter++;
        if (g_current_ability.strike_counter < g_current_ability.strike_count) {
            work->continue_attack = 1;
        } else {
            work->continue_attack = 0;
        }
    }
    return 0;
}
