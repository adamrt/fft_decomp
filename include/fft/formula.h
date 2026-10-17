#ifndef FORMULA_H
#define FORMULA_H

/* Ability formulas: the handler of each formula id and the steps they share to work out an action's damage, accuracy,
 * elements and statuses. They take no arguments and work on g_action_current_ability and the target's action data
 * (action.h). */

#include "psx/types.h"

/* formula handlers */
/* The handlers in table order, indexed by formula id less 1: formula 0 has no handler. */
extern void (*g_formula_handlers[])(void);

/* weapon attacks */
void formula_weapon_damage(void);
void formula_weapon_damage_with_proc(void);
void formula_gun_damage(void);
void formula_magic_gun(void);
void formula_weapon_damage_without_element_or_weather(void);
void formula_weapon_absorb_hp(void);
void formula_healing_staff(void);

/* shared magic, Time Magic, Summon Magic and boss spells */
void formula_magical_damage(void);
void formula_magic_hp_percent_damage(void);
void formula_hit_faith_ma_x_percent(void);
void formula_friendly_magic_accuracy(void);
void formula_magical_heal(void);
void formula_heal_y_percent_faith(void);
void formula_damage_hp_percent_hit_faith_ma_x_percent(void);
void formula_absorb_mp_y_percent(void);
void formula_magic_hp_absorb(void);
void formula_11_unused(void);
void formula_set_quick(void);
void formula_13_unused(void);
void formula_set_golem(void);
void formula_return_2(void);
void formula_mute(void);
void formula_gravi_2(void);
void formula_18_unused(void);
void formula_19_unused(void);

/* Destroy Sword */
void formula_destroy_sword_stat_ruin(void);
void formula_magic_ruin(void);

/* Sing and Dance */
void formula_song(void);
void formula_dance(void);

/* Truth, Holy Bracelet and Un-truth */
void formula_multihit_truth_magic(void);
void formula_un_truth(void);

/* Draw Out */
void formula_draw_out_damage(void);
void formula_bizen_boat(void);
void formula_draw_out_status(void);
void formula_murasame(void);

/* Elemental */
void formula_elemental(void);

/* Battle Skill: equipment breaks */
void formula_battle_skill_equipment_break(void);

/* Steal and shared gil-taking / Charm abilities */
void formula_steal_equipment(void);
void formula_steal_gil(void);
void formula_steal_exp(void);
void formula_charm(void);

/* Talk Skill */
void formula_talk_skill(void);

/* Battle Skill: stat breaks */
void formula_battle_skill_stat_break(void);
void formula_magic_break(void);

/* Holy Sword and Mighty Sword */
void formula_holy_sword(void);
void formula_mighty_sword(void);

/* Dark Sword */
void formula_dark_sword(void);
void formula_night_sword(void);

/* Punch Art and shared abilities */
void formula_damage_pa_plus_y_half_times_pa(void);
void formula_repeating_fist(void);
void formula_stigma_magic(void);
void formula_chakra(void);
void formula_heal_y_percent_hit_pa_x_percent(void);

/* Basic Skill and shared stat / status abilities */
void formula_add_pa_y(void);
void formula_damage_random_y_times_pa(void);
void formula_apply_status_to_action(void);

/* Guts and shared Dragon / Byblos helpers */
void formula_yell(void);
void formula_cheer_up(void);
void formula_add_brave_x_stats_y(void);
void formula_sacrifice_hp_to_heal(void);

/* other status and damage formulas */
void formula_hit_ma_x_percent_magical_evade(void);
void formula_damage_target_hp_minus_one_always_hit(void);

/* Snipe */
void formula_snipe_aim(void);
void formula_seal_evil(void);

/* Starry Heaven and Work */
void formula_galaxy_stop(void);
void formula_work(void);

/* Byblos, Limit and shared special abilities */
void formula_damage_caster_missing_hp(void);
void formula_difference(void);
void formula_climhazzard(void);
void formula_46_unused(void);
void formula_blood_suck(void);

/* Item */
void formula_potion(void);
void formula_ether(void);
void formula_elixir(void);
void formula_phoenix_down(void);

/* monster abilities and shared Limit, Breath and Punch Art formulas */
void formula_heal_ma_times_y(void);
void formula_absorb_hp_y_percent_hit_ma_x_percent(void);
void formula_damage_ma_times_y(void);
void formula_goblin_punch(void);
void formula_hit_ma_x_percent_physical_evade(void);
void formula_hit_ma_x_percent_no_evade(void);
void formula_self_destruct(void);
void formula_damage_y_percent_hit_ma_x_percent(void);
void formula_magic_spirit(void);
void formula_beaking(void);
void formula_circle(void);
void formula_please_eat(void);
void formula_moldball_virus(void);
void formula_level_blast(void);

/* Dragon */
void formula_dragon_tame(void);
void formula_dragon_care(void);
void formula_dragon_power_up(void);
void formula_dragon_level_up(void);

/* monster abilities, Night Magic and shared Brave reduction */
void formula_fixed_multihit_magic(void);
void formula_nanoflare(void);
void formula_damage_ma_plus_y_half_times_ma_status(void);
void formula_lower_brave_y(void);
void formula_look_of_fright(void);

/* Throw and Jump */
void formula_throw_damage(void);
void formula_jump_damage(void);

/* A second entry to formula 0x49's handler that the table does not point to and nothing calls. */
void formula_49_unused_with_extra_steps(void);

/* damage and stat modifiers */
void formula_apply_attack_up_and_martial_arts(void);
void formula_apply_attacker_berserk_frog(void);
void formula_apply_charge(void);
void formula_apply_damage_and_knockback(void);
void formula_apply_defense_up(void);
void formula_apply_full_hp_mp_heal(void);
void formula_apply_hp_absorption(void);
void formula_apply_magic_attack_up(void);
void formula_apply_magic_defense_up(void);
void formula_apply_mp_absorption(void);
void formula_apply_physical_attack_supports(void);
void formula_set_brave_loss_y(void);
void formula_calculate_base_xa(void);
void formula_calculate_damage_leaving_one_hp(void);
void formula_calculate_hp_percent_damage(void);
void formula_calculate_magical_xa_times_ya(void);
void formula_calculate_mp_percent_damage(void);
void formula_calculate_physical_damage(void);
void formula_calculate_stolen_gil(void);
void formula_calculate_truth_damage(void);
void formula_calculate_magical_damage_without_faith(void);
void formula_convert_hp_damage_to_mp_recovery(void);
void formula_set_exp_stolen(void);
void formula_store_ma_and_ma_plus_y_divided_by_two(void);
void formula_store_ma_and_pa_plus_y_divided_by_two(void);
void formula_store_ma_and_x(void);
void formula_store_ma_and_y(void);
void formula_store_pa_and_pa_plus_y_divided_by_two(void);
void formula_store_pa_and_x(void);
void formula_store_pa_and_y(void);
void formula_store_speed_and_x(void);
void formula_store_xa_times_ya_damage(void);
void formula_apply_poach_and_train(void);

/* accuracy and evasion */
/* Chances in 100 of a critical hit and of a weapon's added effect. */
enum { FORMULA_CRITICAL_HIT_CHANCE = 4, FORMULA_ADDED_EFFECT_CHANCE = 19 };

typedef enum formula_facing_modifier {
    FORMULA_FACING_FRONT = 0,
    FORMULA_FACING_SIDE = 1,
    FORMULA_FACING_BACK = 2
} formula_facing_modifier_e;

void formula_evade_apply_target_statuses(void);
void formula_evade_calculate_abandon(void);
void formula_evade_calculate_concentrate(void);
void formula_calculate_critical_hit(void);
void formula_evade_calculate_dark_confuse(void);
void formula_evade_calculate_facing(void);
void formula_evade_calculate_final_hit_percent(void);
s32 formula_accuracy_calculate_magic_without_faith(void);
s32 formula_evade_calculate_magical(void);
s32 formula_evade_calculate_physical(void);
void formula_evade_calculate_transparent(void);
s32 formula_evade_calculate_physical_without_weather(void);
void formula_evade_store_magical(void);
void formula_use_hp_damage_as_action_hit_percent(void);
void formula_apply_zodiac_compatibility(void);
void formula_evade_store_physical(void);
s32 formula_calculate_dance_song_hit(void);
s32 formula_accuracy_calculate_magic(void);
s32 formula_accuracy_calculate_physical(void);
s32 formula_accuracy_calculate_physical_status(void);

/* elements and faith */
void formula_element_apply(void);
void formula_element_apply_absorption(void);
s32 formula_element_apply_absorption_and_status(void);
s32 formula_element_apply_absorption_and_status_proc(void);
void formula_element_apply_strengthen(void);
s32 formula_apply_status_and_check_undead(void);
void formula_apply_undead_absorb_attack(void);
void formula_apply_undead_reversal(void);
void formula_element_apply_weapon_strengthen(void);
void formula_evade_apply_weather_on_bows(void);
void formula_element_apply_weather(void);
void formula_calculate_faith(void);
void formula_element_modify_damage(s32 element);
void formula_element_modify_weapon_damage(void);
s32 formula_calculate_elemental_xa_times_ya(void);

/* statuses */
void formula_apply_dance_abilities(void);
void formula_apply_magical_modifiers(void);
void formula_apply_physical_modifiers_without_supports(void);
void formula_apply_physical_modifiers(void);
void formula_apply_quick_effect(void);
void formula_apply_song_abilities(void);
void formula_apply_status(void);
void formula_apply_target_magical_statuses(void);
void formula_apply_target_physical_statuses(void);
void formula_apply_talk_skill(void);
void formula_check_dragon(void);
void formula_force_sleeping_target_miss(void);
s32 formula_roll_conditional_status_proc(void);
void formula_store_xa_plus_ya(void);

/* equipment and abilities */
void formula_apply_maintenance(void);
s32 formula_select_targeted_equipment(void);
void formula_calculate_katana_break_chance(void);
void formula_store_jump_xa_ya(void);
void formula_select_magic_gun_ability(void);
void formula_store_pa_and_weapon_power(void);
void formula_store_pa_and_weapon_power_plus_y(void);
s32 formula_calculate_weapon_damage(void);

/* results and random */
void formula_nullify_action(void);
void formula_cause_action_miss(void);
void formula_force_attack_miss(void);
void formula_clear_pending_proc(void);
void formula_queue_weapon_spell(void);
void formula_determine_reduced_stat(void);

/* rand() while the action executes; the midpoint 0x4000 for previews and AI simulations. */
s32 formula_get_random(void);
s32 formula_roll_random_at_least(s32 limit, s32 threshold);

#endif
