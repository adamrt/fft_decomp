/* Ability formulas for talk skills, physical stat reduction, PA and WP based damage, absorption and healing, level
 * changes, dragon abilities and brave loss. */

#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/data.h"
#include "fft/main_item.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Formula 0x2A: talk skill, hit (MA+X)%: gil (Level * SP), CT to 0, +/-Brave
 * or Faith (Y), or status by ability. Misses a sleeping target and, without
 * Monster Talk, a monster; Finger Guard can cancel it. */
void battle_formula_talk_skill_hit_ma_x_percent(void) {
    battle_formula_force_sleeping_target_miss();
    if ((g_battle_action_target->unit_flags & UNIT_FLAG_MONSTER)
        && !(g_battle_action_attacker->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_MONSTER_TALK)) {
        battle_formula_force_attack_miss();
    }
    if (g_battle_action_target_data->hit != 0) {
        battle_formula_apply_finger_guard();
        if (g_battle_action_target_data->hit != 0) {
            battle_formula_store_ma_and_x();
            battle_formula_apply_zodiac_compatibility();
            battle_formula_store_xa_plus_ya_status_damage();
            battle_formula_use_hp_damage_as_action_hit_percent();
            if (g_battle_action_target_data->hit != 0) {
                battle_formula_apply_talk_skill();
            }
        }
    }
}

/* Formula 0x2B: -PA, -MA or -Speed (X) by ability, hit (PA+Y)%; physical
 * evade. */
void battle_formula_physical_stat_reduction(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_pa_and_y();
        if (battle_formula_calculate_physical_accuracy() == 0) {
            battle_formula_determine_reduced_stat();
        }
    }
}

/* Formula 0x2C: MP damage (Y)% of max MP, hit (PA+Y)%; physical evade. */
void battle_formula_physical_mp_percent_damage(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_pa_and_y();
        if (battle_formula_calculate_physical_accuracy() == 0) {
            battle_formula_calculate_mp_percent_damage();
        }
    }
}

/* Formula 0x2D: damage (PA * (WP + Y)), 100% status. */
void battle_formula_damage_pa_times_wp_plus_y_status(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_pa_and_weapon_power_plus_y();
        battle_formula_apply_weapon_element_strengthen();
        battle_formula_calculate_physical_damage();
        battle_formula_modify_elemental_damage();
        if (g_battle_action_target_data->hit != 0) {
            battle_formula_apply_elemental_absorption();
            battle_formula_apply_status();
        }
    }
}

/* Formula 0x2E: break the equipment the ability names and damage (PA * WP)
 * with the weapon's element; physical evade. Fails with nothing in that slot
 * or against Maintenance. */
void battle_formula_break_equipped_damage_pa_times_wp(void) {
    action_data_t* action;

    if (battle_formula_calculate_physical_evade() != 0) {
        return;
    }
    if (battle_formula_apply_steal_break_might_sword_hardcoding() != 0) {
        battle_formula_nullify_action();
        g_battle_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
        return;
    }
    battle_formula_apply_maintenance();
    action = g_battle_action_target_data;
    if (action->hit == 0) {
        return;
    }
    /* The target writes the combined special-effect field as one 16-bit value. */
    action->special_effect = ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT;
    battle_formula_store_pa_and_weapon_power();
    battle_formula_apply_weapon_element_strengthen();
    battle_formula_calculate_physical_damage();
    battle_formula_modify_elemental_damage();
    if (g_battle_action_target_data->hit == 0) {
        return;
    }
    battle_formula_apply_elemental_absorption();
}

/* Formula 0x2F: absorb (PA * WP) MP; physical evade. */
void battle_formula_absorb_mp_pa_times_wp(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_pa_and_weapon_power();
        battle_formula_calculate_physical_damage();
        battle_formula_apply_mp_recovery();
    }
}

/* Formula 0x30: absorb (PA * WP) HP; physical evade. */
void battle_formula_absorb_hp_pa_times_wp(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_pa_and_weapon_power();
        battle_formula_calculate_physical_damage();
        battle_formula_apply_hp_absorption();
    }
}

/* Formula 0x31: damage ((PA + Y) / 2 * PA) with element; physical evade; 19%
 * status. */
void battle_formula_damage_pa_plus_y_half_times_pa(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_pa_and_pa_plus_y_divided_by_two();
        battle_formula_apply_elemental_strengthen();
        battle_formula_apply_attack_up_and_martial_arts();
        battle_formula_apply_physical_status_support_compatibility();
        battle_formula_calculate_critical_hit();
        battle_formula_store_xa_times_ya_damage();
        battle_formula_apply_weather_elemental_effects();
        battle_formula_apply_elemental();
        if (g_battle_action_target_data->hit != 0) {
            battle_formula_apply_elemental_absorption();
            if (battle_formula_roll_conditional_status_proc() == 0) {
                battle_formula_apply_status();
            }
        }
    }
}

/* Formula 0x32: damage ((PA + (PA + Y) / 2) * rnd(1..X)); physical evade. */
void battle_formula_damage_random_x_times_pa_plus_y_half_times_pa(void) {
    action_data_t* action;
    u8* mult;
    s32 rnd;

    if (battle_formula_calculate_physical_evade() != 0)
        return;
    battle_formula_store_pa_and_pa_plus_y_divided_by_two();
    battle_formula_apply_attack_up_and_martial_arts();
    battle_formula_apply_physical_status_support_compatibility();
    battle_formula_calculate_critical_hit();
    battle_formula_store_xa_plus_ya_status_damage();
    rnd = battle_formula_get_random_0_7fff();
    mult = &g_battle_action_current_ability.random_damage_factor;
    *mult = (rnd * g_battle_action_current_ability.range_data.x) / 32768 + 1;
    action = g_battle_action_target_data;
    /* The target loads hp_damage signed (lh) here; keep the cast and the
       multiplier deref separated so `* *` never reads as a comment. */
    action->hp_damage = action->hp_damage * (*mult);
    action->attack_type = ACTION_TYPE_HP_DAMAGE;
}

/* Formula 0x33: hit (PA+X)%. */
void battle_formula_hit_pa_x_percent(void) {
    battle_formula_store_pa_and_x();
    battle_formula_apply_attack_up_and_martial_arts();
    battle_formula_apply_attacker_berserk_frog();
    battle_formula_apply_zodiac_compatibility();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_use_hp_damage_as_action_hit_percent();
    if (g_battle_action_target_data->hit != 0) {
        battle_formula_apply_status_to_action();
    }
}

/* Formula 0x34: heal (PA*Y) HP and (PA*Y/2) MP. */
void battle_formula_heal_pa_times_y_and_mp(void) {
    action_data_t* action;
    u16 amount;

    battle_formula_store_pa_and_y();
    battle_formula_apply_attack_up_and_martial_arts();
    battle_formula_apply_zodiac_compatibility();
    battle_formula_store_xa_times_ya_damage();
    action = g_battle_action_target_data;
    amount = action->hp_damage;
    action->hp_damage = 0;
    action->hp_healing = amount;
    action->attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_MP_HEALING;
    /* hp_healing is re-read signed (lh) for the halving. */
    action->mp_healing = action->hp_healing / 2;
}

/* Formula 0x35: heal (Y)% of max HP, hit (PA+X)%. */
void battle_formula_heal_y_percent_hit_pa_x_percent(void) {
    battle_formula_store_pa_and_x();
    battle_formula_apply_attack_up_and_martial_arts();
    battle_formula_apply_zodiac_compatibility();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_use_hp_damage_as_action_hit_percent();
    if (g_battle_action_target_data->hit != 0 && battle_formula_apply_status_and_check_undead() != 0) {
        battle_formula_calculate_hp_percent_damage();
        battle_formula_apply_undead_reversal();
    }
}

/* Formula 0x37: damage (PA * rnd(1..Y)); physical evade. Knockback when a roll
 * below the target's Brave is under a roll below the attacker's Brave. */
void battle_formula_damage_random_y_times_pa(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_pa_and_y();
        battle_formula_apply_attack_up_and_martial_arts();
        battle_formula_apply_physical_status_support_compatibility();
        battle_formula_apply_damage_and_knockback();
    }
}

/* Formula 0x3D: status, hit (MA+X)%; magic evade. */
void battle_formula_hit_ma_x_percent_magical_evade(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        if (battle_formula_calculate_magic_accuracy_without_faith() == 0) {
            battle_formula_apply_status_to_action();
        }
    }
}

/* Formula 0x3E: damage equal to the target's current HP - 1 (0 when it has none). */
void battle_formula_damage_target_hp_minus_one_always_hit(void) {
    battle_formula_calculate_gravi2_damage();
}

/* Formula 0x3F: hit (SP+X)%. */
void battle_formula_hit_sp_x_percent(void) {
    battle_formula_store_speed_and_x();
    battle_formula_apply_physical_xa_modifying_statuses_support();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_use_hp_damage_as_action_hit_percent();
    if (g_battle_action_target_data->hit != 0) {
        battle_formula_apply_status_to_action();
    }
}

/* Formula 0x40: undead targets only, hit (SP+X)%. */
void battle_formula_undead_hit_sp_x_percent(void) {
    battle_formula_store_speed_and_x();
    battle_formula_apply_physical_xa_modifying_statuses_support();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_use_hp_damage_as_action_hit_percent();
    if (g_battle_action_target_data->hit != 0) {
        if ((g_battle_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
                & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))
            == 0) {
            battle_formula_force_attack_miss();
        } else {
            battle_formula_apply_status_to_action();
        }
    }
}

/* Formula 0x41: hit (MA+X)%; forced miss when caster and target share a zodiac sign. */
void battle_formula_hit_ma_x_percent_miss_same_zodiac(void) {
    if (battle_formula_calculate_magic_accuracy_without_faith() == 0) {
        if (g_battle_action_target->birthday.zodiac == g_battle_action_attacker->birthday.zodiac) {
            battle_formula_force_attack_miss();
        } else {
            battle_formula_apply_status_to_action();
        }
    }
}

/* Formula 0x42: damage (PA * Y); the caster takes (PA * Y / X). No evade. */
void battle_formula_damage_pa_times_y_damage_caster_pa_times_y_over_x(void) {
    s16 damage;
    action_data_t* action;

    battle_formula_store_pa_and_y();
    battle_formula_apply_zodiac_compatibility();
    damage = g_battle_action_current_ability.xa * g_battle_action_current_ability.ya;
    /* Cast-pointer stores: as struct members GCC sinks them past the
     * following scalar global loads; the target keeps source order. */
    g_battle_action_target_data->hp_damage = damage;
    damage = damage / g_battle_action_current_ability.range_data.x;
    action = g_battle_action_attacker_data;
    action->hit = 1;
    action->hp_damage = damage;
    g_battle_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
    g_battle_action_attacker_data->attack_type = ACTION_TYPE_HP_DAMAGE;
}

/* Formula 0x46: an empty handler in g_battle_formula_handlers. */
void battle_formula_46_unused(void) {
}

/* Formula 0x47: absorb (Y)% of max HP, 100% status. */
void battle_formula_absorb_hp_y_percent_status(void) {
    battle_formula_calculate_hp_percent_damage();
    battle_formula_apply_hp_absorption();
    battle_formula_apply_status();
}

/* Formula 0x48: heal (Z * 10) HP. */
void battle_formula_heal_z_times_ten(void) {
    g_battle_action_target_data->hp_damage
        = g_main_item_secondary_data[g_battle_action_current_ability.used_item_id].z * 10;
    battle_formula_apply_undead_reversal();
}

/* A second entry to formula 0x49's handler. The formula table does not point
 * here and no retail code calls it. */
void battle_formula_49_unused_with_extra_steps(void) {
    battle_formula_heal_mp_z_times_ten();
}

/* Formula 0x4A: heal 100% HP and 100% MP. */
void battle_formula_heal_full_hp_mp(void) {
    battle_formula_apply_full_hp_mp_heal();
    battle_formula_apply_undead_reversal();
}

/* Formula 0x4B: heal rnd(1..Z) HP once its status applies; an undead target
 * takes its current HP as damage instead. */
void battle_formula_heal_random_one_to_z_add_status(void) {
    unit_stats_t* unit;

    if (battle_formula_apply_status_and_check_undead() == 0)
        return;
    unit = g_battle_action_target;
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) != 0) {
        g_battle_action_target_data->hp_damage = unit->hp;
    } else {
        g_battle_action_target_data->hp_damage
            = (battle_formula_get_random_0_7fff()
                  * g_main_item_secondary_data[g_battle_action_current_ability.used_item_id].z)
                / 32768
            + 1;
    }
    battle_formula_apply_undead_reversal();
}

/* Formula 0x4C: heal (MA * Y) HP. */
void battle_formula_heal_ma_times_y(void) {
    battle_formula_store_ma_and_y();
    battle_formula_calculate_magical_xa_times_ya();
    battle_formula_apply_undead_reversal();
}

/* Formula 0x4D: absorb (Y)% of max HP, hit (MA+X)%. */
void battle_formula_absorb_hp_y_percent_hit_ma_x_percent(void) {
    if (battle_formula_calculate_physical_evade() != 0) {
        return;
    }
    battle_formula_store_ma_and_x();
    if (battle_formula_calculate_physical_status_accuracy() != 0) {
        return;
    }
    battle_formula_calculate_hp_percent_damage();
    battle_formula_apply_hp_absorption();
}

/* Formula 0x4E: damage (MA * Y). */
void battle_formula_damage_ma_times_y(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        battle_formula_store_ma_and_y();
        battle_formula_calculate_truth_magical_damage();
    }
}

/* Formula 0x4F: damage equal to the caster's missing HP, hit (MA+X)%; physical
 * evade. */
void battle_formula_damage_caster_missing_hp(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_ma_and_x();
        if (battle_formula_calculate_physical_status_accuracy() == 0) {
            battle_formula_43_damage_caster_missing_hp();
        }
    }
}

/* Formula 0x50: status, hit (MA+X)%; physical evade. */
void battle_formula_hit_ma_x_percent_physical_evade(void) {
    if (battle_formula_calculate_physical_evade() == 0) {
        battle_formula_store_ma_and_x();
        if (battle_formula_calculate_physical_status_accuracy() == 0) {
            battle_formula_apply_status_to_action();
        }
    }
}

/* Formula 0x51: hit (MA+X)%. */
void battle_formula_hit_ma_x_percent_no_evade(void) {
    battle_formula_store_ma_and_x();
    battle_formula_apply_elemental_strengthen();
    battle_formula_apply_magic_attack_up();
    battle_formula_apply_zodiac_compatibility();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_use_hp_damage_as_action_hit_percent();
    if (g_battle_action_target_data->hit != 0) {
        battle_formula_apply_status_to_action();
    }
}

/* Formula 0x52: damage (CasMaxHP - CasCurHP), 100% status; caster in AoE takes its own current HP as damage. */
void battle_formula_damage_caster_missing_hp_self_sacrifice(void) {
    if (g_battle_action_target != g_battle_action_attacker) {
        unit_stats_t* caster = g_battle_action_attacker;
        action_data_t* action = g_battle_action_target_data;

        action->hp_damage = caster->max_hp - caster->hp;
        action->attack_type = ACTION_TYPE_HP_DAMAGE;
    } else {
        action_data_t* action = g_battle_action_target_data;

        action->hp_damage = g_battle_action_target->hp;
        action->attack_type = ACTION_TYPE_HP_DAMAGE;
    }
    if (g_battle_action_target != g_battle_action_attacker) {
        battle_formula_apply_status();
    }
}

/* Formula 0x53: damage (Y)% of max HP, hit (MA+X)%. */
void battle_formula_damage_y_percent_hit_ma_x_percent(void) {
    if (battle_formula_calculate_magical_evade() != 0) {
        return;
    }
    if (battle_formula_calculate_magic_accuracy_without_faith() != 0) {
        return;
    }
    battle_formula_calculate_hp_percent_damage();
    battle_formula_apply_weather_elemental_effects();
    battle_formula_apply_elemental();
    if (g_battle_action_target_data->hit != 0 && battle_formula_apply_elemental_absorption_and_status_proc() == 0) {
        battle_formula_apply_status();
    }
}

/* Formula 0x54: heal (MA * Y) MP. */
void battle_formula_heal_mp_ma_times_y(void) {
    battle_formula_store_ma_and_y();
    battle_formula_calculate_magical_xa_times_ya();
    battle_formula_convert_hp_damage_to_mp_recovery();
}

/* Formula 0x55: -PA (Y), hit (MA+X)%. */
void battle_formula_lower_pa_y(void) {
    if (battle_formula_calculate_magical_evade() != 0 || battle_formula_calculate_magic_accuracy_without_faith() != 0) {
        return;
    }
    g_battle_action_target_data->pa_change
        = g_battle_action_current_ability.range_data.y & ACTION_STAT_CHANGE_VALUE_MASK;
    g_battle_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x56: -MA (Y), hit (MA+X)%. */
void battle_formula_lower_ma_y(void) {
    if (battle_formula_calculate_magical_evade() != 0 || battle_formula_calculate_magic_accuracy_without_faith() != 0) {
        return;
    }
    g_battle_action_target_data->ma_change
        = g_battle_action_current_ability.range_data.y & ACTION_STAT_CHANGE_VALUE_MASK;
    g_battle_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x57: +1 level (a miss at level 99) and the ability's status on the
 * caster. A full HP and MP heal is stored first, but its action type is then
 * overwritten by the level change's. */
void battle_formula_raise_level_by_one_add_status_on_caster(void) {
    action_data_t* action;
    unit_stats_t* saved_unit;
    action_data_t* saved_action;
    unit_stats_t* attacker;
    action_data_t* attacker_action;

    battle_formula_apply_full_hp_mp_heal();
    battle_formula_apply_undead_reversal();
    if (g_battle_action_target->level >= UNIT_LEVEL_MAX) {
        g_battle_action_target_data->attack_type = 0;
        battle_formula_force_attack_miss();
        return;
    }
    action = g_battle_action_target_data;
    action->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    saved_unit = g_battle_action_target;
    saved_action = g_battle_action_target_data;
    attacker = g_battle_action_attacker;
    attacker_action = g_battle_action_attacker_data;
    /* The target writes the combined special-effect field as one 16-bit value. */
    action->special_effect = ACTION_SPECIAL_EFFECT_LEVEL_UP;
    g_battle_action_target = attacker;
    g_battle_action_target_data = attacker_action;
    attacker_action->hit = 1;
    battle_formula_apply_status_to_action();
    g_battle_action_target = saved_unit;
    g_battle_action_target_data = saved_action;
}

/* Formula 0x58 "Set Morbol: Hit(MA+X)%". */

/* Generic character identities are 0x80 male, 0x81 female, 0x82 monster;
 * Morbol-type jobs are 0x82 Morbol, 0x83 Ochu, 0x84 Great Morbol. */
void battle_formula_set_morbol(void) {
    action_data_t* action;
    unit_stats_t* target;

    battle_formula_store_ma_and_x();
    battle_formula_apply_elemental_strengthen();
    battle_formula_apply_magic_attack_up();
    battle_formula_apply_zodiac_compatibility();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_use_hp_damage_as_action_hit_percent();
    action = g_battle_action_target_data;
    if (action->hit == 0)
        return;
    target = g_battle_action_target;
    if (!(target->team_flags & BATTLE_TEAM_FLAG_IMMORTAL) && target->mount_info == 0
        && target->character_identity >= CHARACTER_IDENTITY_GENERIC_MALE
        && target->character_identity <= CHARACTER_IDENTITY_MONSTER
        && (target->job_id < JOB_ID_MORBOL || target->job_id > JOB_ID_MORBOL_FAMILY_LAST)) {
        /* The 16-bit store clears all other special-effect flags. */
        action->special_effect = ACTION_SPECIAL_EFFECT_MORBOL;
        action->attack_type = ACTION_TYPE_PSEUDO_STATUS;
        return;
    }
    battle_formula_force_attack_miss();
}

/* Formula 0x59: -Level (1), hit (MA+X)%. */
void battle_formula_lower_level_by_one(void) {
    action_data_t* action;

    if (battle_formula_calculate_magical_evade() == 0) {
        if (battle_formula_calculate_magic_accuracy_without_faith() == 0) {
            if (g_battle_action_target->level < 2) {
                battle_formula_force_attack_miss();
            }
            action = g_battle_action_target_data;
            /* The target writes the combined special-effect field as one 16-bit value. */
            action->special_effect = ACTION_SPECIAL_EFFECT_LEVEL_DOWN;
            action->attack_type = ACTION_TYPE_PSEUDO_STATUS;
        }
    }
}

/* Formula 0x5A: status, 100%; dragons only (formula_check_dragon). */
void battle_formula_dragon_hit_100(void) {
    battle_formula_check_dragon();
    if (g_battle_action_target_data->hit != 0) {
        battle_formula_apply_status_to_action();
    }
}

/* Formula 0x5B: dragons only; the caster loses MaxHP/5 and the target gains
 * twice that (formula 0x3C), then 100% status. Y is not read. */
void battle_formula_dragon_heal_y_percent_add_status(void) {
    battle_formula_check_dragon();
    if (g_battle_action_target_data->hit != 0) {
        battle_formula_3c_damage_caster_max_hp_one_fifth_heal_target_two_fifths();
        battle_formula_apply_status();
    }
}

/* Formula 0x5C: +Brave (X), +PA/MA/Speed (Y); dragons only. */
void battle_formula_dragon_brave_x_stats_y(void) {
    battle_formula_check_dragon();
    if (g_battle_action_target_data->hit != 0) {
        battle_formula_add_brave_x_stats_y();
    }
}

/* Formula 0x5D: Quick; dragons only. */
void battle_formula_dragon_set_quick(void) {
    battle_formula_check_dragon();
    if (g_battle_action_target_data->hit != 0) {
        battle_formula_apply_quick_effect();
    }
}

/* Formula 0x5E: damage ((MA + Y) / 2 * MA). */
void battle_formula_5e_damage_ma_plus_y_half_times_ma(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        battle_formula_calculate_truth_damage();
    }
}

/* Formula 0x5F: damage ((MA + Y) / 2 * MA). */
void battle_formula_5f_damage_ma_plus_y_half_times_ma(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        battle_formula_calculate_truth_damage();
    }
}

/* Formula 0x60: damage ((MA + Y) / 2 * MA) with element, 19% status; no evade. */
void battle_formula_damage_ma_plus_y_half_times_ma_status(void) {
    battle_formula_calculate_truth_damage();
}

/* Formula 0x61: -Brave (Y), hit F(MA+X)%; magic evade. */
void battle_formula_lower_brave_y(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        if (battle_formula_calculate_magic_accuracy() == 0) {
            battle_formula_set_brave_loss_y();
        }
    }
}

/* Formula 0x62: -Brave (Y), hit (MA+X)%; magic evade. */
void battle_formula_lower_brave_y_without_faith(void) {
    if (battle_formula_calculate_magical_evade() == 0) {
        if (battle_formula_calculate_magic_accuracy_without_faith() == 0) {
            battle_formula_set_brave_loss_y();
        }
    }
}

/* Formula 0x63: thrown weapon damage (SP * WP) with the weapon's element;
 * Catch, then physical evade. */
void battle_formula_damage_sp_times_wp(void) {
    battle_formula_apply_catch();
    if (g_battle_action_target_data->hit == 0) {
        return;
    }
    if (battle_formula_calculate_physical_evade() != 0) {
        return;
    }
    g_battle_action_current_ability.xa = g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED];
    g_battle_action_current_ability.ya = g_battle_action_current_ability.weapon_data.power;
    battle_formula_apply_physical_status_support_compatibility();
    battle_formula_store_xa_times_ya_damage();
    battle_formula_modify_elemental_damage();
    if (g_battle_action_target_data->hit == 0) {
        return;
    }
    battle_formula_apply_elemental_absorption();
}

/* Formula 0x64: damage (PA * WP); with a spear (PA * 3/2 * WP); bare-handed
 * (PA * (PA * Brave / 100)). No evade roll here. */
void battle_formula_damage_pa_times_wp(void) {
    battle_formula_init_weapon_xa_ya();
    battle_formula_apply_physical_status_support_compatibility();
    battle_formula_store_xa_times_ya_damage();
}
