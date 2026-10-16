#ifndef BATTLE_FORMULA_H
#define BATTLE_FORMULA_H

/* Ability formulas: the handler of each formula id and the steps they share to work out an
 * action's damage, accuracy, elements and statuses. They take no arguments and work on
 * g_battle_action_current_ability and the target's action data (battle_action.h). */

#include "psx/types.h"

/* damage and stat modifiers */
void battle_formula_apply_attack_up_and_martial_arts(void);
void battle_formula_apply_attacker_berserk_frog(void);
void battle_formula_apply_charge(void);
void battle_formula_apply_damage_and_knockback(void);
void battle_formula_apply_defense_up(void);
void battle_formula_apply_full_hp_mp_heal(void);
void battle_formula_apply_hp_absorption(void);
void battle_formula_apply_magic_attack_up(void);
void battle_formula_apply_magic_defense_up(void);
void battle_formula_apply_mp_absorption(void);
void battle_formula_apply_physical_attack_supports(void);
void battle_formula_set_brave_loss_y(void);
void battle_formula_calculate_base_xa(void);
void battle_formula_calculate_damage_leaving_one_hp(void);
void battle_formula_calculate_hp_percent_damage(void);
void battle_formula_calculate_magical_xa_times_ya(void);
void battle_formula_calculate_mp_percent_damage(void);
void battle_formula_calculate_physical_damage(void);
void battle_formula_calculate_stolen_gil(void);
void battle_formula_calculate_truth_damage(void);
void battle_formula_calculate_magical_damage_without_faith(void);
void battle_formula_convert_hp_damage_to_mp_recovery(void);
void battle_formula_set_exp_stolen(void);
void battle_formula_store_ma_and_ma_plus_y_divided_by_two(void);
void battle_formula_store_ma_and_pa_plus_y_divided_by_two(void);
void battle_formula_store_ma_and_x(void);
void battle_formula_store_ma_and_y(void);
void battle_formula_store_pa_and_pa_plus_y_divided_by_two(void);
void battle_formula_store_pa_and_x(void);
void battle_formula_store_pa_and_y(void);
void battle_formula_store_speed_and_x(void);
void battle_formula_store_xa_times_ya_damage(void);
void battle_formula_apply_poach_and_train(void);

/* accuracy and evasion */
/* Chances in 100 of a critical hit and of a weapon's added effect. */
enum {
    BATTLE_FORMULA_CRITICAL_HIT_CHANCE = 4,
    BATTLE_FORMULA_ADDED_EFFECT_CHANCE = 19,
};

typedef enum battle_ability_facing_modifier {
    BATTLE_ABILITY_FACING_FRONT = 0,
    BATTLE_ABILITY_FACING_SIDE = 1,
    BATTLE_ABILITY_FACING_BACK = 2,
} battle_ability_facing_modifier_e;

/* The compatibility table is indexed by absolute zodiac-ID difference, not
 * by a particular sign. Value 3 is resolved from sex and monster flags. */
typedef enum zodiac_compatibility {
    ZODIAC_COMPATIBILITY_NEUTRAL = 0,
    ZODIAC_COMPATIBILITY_BAD = 1,
    ZODIAC_COMPATIBILITY_GOOD = 2,
    ZODIAC_COMPATIBILITY_GENDER_DEPENDENT = 3,
    ZODIAC_COMPATIBILITY_WORST = 4,
    ZODIAC_COMPATIBILITY_BEST = 5,
} zodiac_compatibility_e;

/* Zodiac compatibility by the absolute difference of the two signs, 12 entries:
 * 3 and 9 apart are bad, 4 and 8 good, 6 (the opposite sign) depends on sex,
 * the rest neutral. Serpentarius returns before the table is read. */
extern u8 g_battle_zodiac_compatibility_modifiers[];

void battle_formula_apply_evasion_changes_due_to_statuses(void);
void battle_formula_calculate_abandon(void);
void battle_formula_calculate_concentrate(void);
void battle_formula_calculate_critical_hit(void);
void battle_formula_calculate_dark_confuse(void);
void battle_formula_calculate_facing_evade(void);
void battle_formula_calculate_final_hit_percent(void);
s32 battle_formula_calculate_magic_accuracy_without_faith(void);
s32 battle_formula_calculate_magical_evade(void);
s32 battle_formula_calculate_physical_evade(void);
void battle_formula_calculate_transparent(void);
s32 battle_formula_calculate_physical_evade_without_weather(void);
void battle_formula_store_magical_evade_values(void);
void battle_formula_use_hp_damage_as_action_hit_percent(void);
void battle_formula_apply_zodiac_compatibility(void);
void battle_formula_store_physical_evade_values(void);
s32 battle_formula_calculate_dance_song_hit(void);
s32 battle_formula_calculate_magic_accuracy(void);
s32 battle_formula_calculate_physical_accuracy(void);
s32 battle_formula_calculate_physical_status_accuracy(void);

/* elements and faith */
void battle_formula_apply_elemental(void);
void battle_formula_apply_elemental_absorption(void);
s32 battle_formula_apply_elemental_absorption_and_status(void);
s32 battle_formula_apply_elemental_absorption_and_status_proc(void);
void battle_formula_apply_elemental_strengthen(void);
s32 battle_formula_apply_status_and_check_undead(void);
void battle_formula_apply_undead_absorb_attack(void);
void battle_formula_apply_undead_reversal(void);
void battle_formula_apply_weapon_element_strengthen(void);
void battle_formula_apply_weather_effects_on_bows(void);
void battle_formula_apply_weather_elemental_effects(void);
void battle_formula_calculate_faith(void);
void battle_formula_modify_damage_for_element(s32 element);
void battle_formula_modify_elemental_damage(void);
s32 battle_formula_calculate_elemental_xa_times_ya(void);

/* statuses */
void battle_formula_apply_dance_abilities(void);
void battle_formula_apply_magical_modifiers(void);
void battle_formula_apply_physical_modifiers_without_supports(void);
void battle_formula_apply_physical_modifiers(void);
void battle_formula_apply_quick_effect(void);
void battle_formula_apply_song_abilities(void);
void battle_formula_apply_status(void);
void battle_formula_apply_target_magical_statuses(void);
void battle_formula_apply_target_physical_statuses(void);
void battle_formula_apply_talk_skill(void);
void battle_formula_check_dragon(void);
void battle_formula_force_sleeping_target_miss(void);
s32 battle_formula_roll_conditional_status_proc(void);
void battle_formula_store_hit_chance(void);

/* equipment and abilities */
void battle_formula_apply_catch(void);
void battle_formula_apply_finger_guard(void);
void battle_formula_apply_maintenance(void);
s32 battle_formula_select_targeted_equipment(void);
void battle_formula_calculate_katana_break_chance(void);
void battle_formula_init_weapon_xa_ya(void);
void battle_formula_select_magic_gun_ability(void);
void battle_formula_store_pa_and_weapon_power(void);
void battle_formula_store_pa_and_weapon_power_plus_y(void);
s32 battle_formula_calculate_weapon_damage(void);

/* results and random */
void battle_formula_nullify_action(void);
void battle_formula_cause_action_miss(void);
void battle_formula_clear_pending_proc(void);
void battle_formula_determine_reduced_stat(void);
void battle_formula_force_attack_miss(void);
s32 battle_formula_get_random_0_7fff(void);
s32 battle_formula_roll_random_at_least(s32 limit, s32 threshold);
void battle_formula_queue_weapon_spell(void);

/* formula handlers */
/* The handlers in table order, indexed by formula id less 1: formula 0 has no handler. */
extern void (*g_battle_formula_handlers[])(void);

void battle_formula_weapon_damage(void);
void battle_formula_weapon_damage_with_proc(void);
void battle_formula_gun_damage(void);
void battle_formula_magic_gun(void);
void battle_formula_weapon_damage_charge(void);
void battle_formula_weapon_absorb_hp(void);
void battle_formula_weapon_heal(void);
void battle_formula_magical_damage(void);
void battle_formula_magic_hp_percent_damage(void);
void battle_formula_hit_faith_ma_x_percent(void);
void battle_formula_friendly_magic_accuracy(void);
void battle_formula_magical_heal(void);
void battle_formula_heal_y_percent_faith(void);
void battle_formula_damage_hp_percent_hit_faith_ma_x_percent(void);
void battle_formula_absorb_mp_y_percent(void);
void battle_formula_magic_hp_absorb(void);
void battle_formula_11_unused(void);
void battle_formula_set_quick(void);
void battle_formula_13_unused(void);
void battle_formula_set_golem(void);
void battle_formula_set_ct_zero(void);
void battle_formula_damage_mp_target_current_mp(void);
void battle_formula_damage_target_hp_minus_one(void);
void battle_formula_18_unused(void);
void battle_formula_19_unused(void);
void battle_formula_magical_stat_reduction(void);
void battle_formula_damage_mp_percent(void);
void battle_formula_song(void);
void battle_formula_dance(void);
void battle_formula_multihit_truth_magic(void);
void battle_formula_unfaith_magical_damage(void);
void battle_formula_draw_out_damage(void);
void battle_formula_draw_out_mp_damage(void);
void battle_formula_draw_out_status(void);
void battle_formula_draw_out_heal(void);
void battle_formula_ma_pa_half_damage(void);
void battle_formula_break_equipped_hit_pa_wp_y_percent(void);
void battle_formula_steal_equipment(void);
void battle_formula_steal_gil(void);
void battle_formula_steal_exp_hit_sp_x_percent(void);
void battle_formula_opposite_sex_hit_ma_x_percent(void);
void battle_formula_talk_skill_hit_ma_x_percent(void);
void battle_formula_physical_stat_reduction(void);
void battle_formula_physical_mp_percent_damage(void);
void battle_formula_damage_pa_times_wp_plus_y_status(void);
void battle_formula_break_equipped_damage_pa_times_wp(void);
void battle_formula_absorb_mp_pa_times_wp(void);
void battle_formula_absorb_hp_pa_times_wp(void);
void battle_formula_damage_pa_plus_y_half_times_pa(void);
void battle_formula_damage_random_x_times_pa_plus_y_half_times_pa(void);
void battle_formula_hit_pa_x_percent(void);
void battle_formula_heal_pa_times_y_and_mp(void);
void battle_formula_heal_y_percent_hit_pa_x_percent(void);
void battle_formula_add_pa_y(void);
void battle_formula_damage_random_y_times_pa(void);
void battle_formula_apply_status_to_action(void);
void battle_formula_add_speed_y(void);
void battle_formula_add_brave_y(void);
void battle_formula_add_brave_x_stats_y(void);
void battle_formula_3c_damage_caster_max_hp_one_fifth_heal_target_two_fifths(void);
void battle_formula_hit_ma_x_percent_magical_evade(void);
void battle_formula_damage_target_hp_minus_one_always_hit(void);
void battle_formula_hit_sp_x_percent(void);
void battle_formula_undead_hit_sp_x_percent(void);
void battle_formula_hit_ma_x_percent_miss_same_zodiac(void);
void battle_formula_damage_pa_times_y_damage_caster_pa_times_y_over_x(void);
void battle_formula_43_damage_caster_missing_hp(void);
void battle_formula_damage_hp_target_current_mp(void);
void battle_formula_damage_target_missing_hp(void);
void battle_formula_46_unused(void);
void battle_formula_absorb_hp_y_percent_status(void);
void battle_formula_heal_z_times_ten(void);
void battle_formula_heal_mp_z_times_ten(void);
void battle_formula_heal_full_hp_mp(void);
void battle_formula_heal_random_one_to_z_add_status(void);
void battle_formula_heal_ma_times_y(void);
void battle_formula_absorb_hp_y_percent_hit_ma_x_percent(void);
void battle_formula_damage_ma_times_y(void);
void battle_formula_damage_caster_missing_hp(void);
void battle_formula_hit_ma_x_percent_physical_evade(void);
void battle_formula_hit_ma_x_percent_no_evade(void);
void battle_formula_damage_caster_missing_hp_self_sacrifice(void);
void battle_formula_damage_y_percent_hit_ma_x_percent(void);
void battle_formula_heal_mp_ma_times_y(void);
void battle_formula_lower_pa_y(void);
void battle_formula_lower_ma_y(void);
void battle_formula_raise_level_by_one_add_status_on_caster(void);
void battle_formula_set_morbol(void);
void battle_formula_lower_level_by_one(void);
void battle_formula_dragon_hit_100(void);
void battle_formula_dragon_transfer_caster_hp_add_status(void);
void battle_formula_dragon_brave_x_stats_y(void);
void battle_formula_dragon_set_quick(void);
void battle_formula_5e_damage_ma_plus_y_half_times_ma(void);
void battle_formula_5f_damage_ma_plus_y_half_times_ma(void);
void battle_formula_damage_ma_plus_y_half_times_ma_status(void);
void battle_formula_lower_brave_y(void);
void battle_formula_lower_brave_y_without_faith(void);
void battle_formula_damage_sp_times_wp(void);
void battle_formula_damage_pa_times_wp(void);
/* A second entry to formula 0x49's handler that the table does not point to and nothing calls. */
void battle_formula_49_unused_with_extra_steps(void);

#endif
