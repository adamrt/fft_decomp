#ifndef BATTLE_UNIT_ENCOUNTER_H
#define BATTLE_UNIT_ENCOUNTER_H

/* BATTLE unit encounters, deployment, progression and pickups. */

#include "fft/battle_unit.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "psx/types.h"

/* encounters and deployment */
extern s32 g_battle_entd_selection_mode; /* battle_entd_selection_e */
/* Count and coordinates of the tiles already handed out as dismount
 * destinations this action. */
extern s32 g_battle_relocated_unit_count;
extern battle_tile_coordinates_t g_battle_relocated_unit_coords[];
extern s16 g_battle_unit_entd_spritesheet_ids[];
extern s32 g_battle_unit_entd_spritesheet_count;
unit_stats_t* battle_unit_find_stats_by_entd_unit_id(s32 unit_id, s32* out_battle_id);
void battle_unit_load_entd_units(entd_encounter_t* entd_data, s32 mode);
s32 battle_unit_set_enemy_level_data_by_battle_id(u32 battle_id);
s32 battle_unit_init_party_member_battle_data(s32 unused0, s32 unused1, s32 unit_index);
s32 battle_unit_place_in_new_location(u32 runtime_id, map_tile_coordinates_t* tile, s32 facing);
void battle_unit_set_enemy_level_data(unit_stats_t* unit);
s32 battle_unit_find_relocation_tile(s32 unit_index, map_tile_coordinates_t* destination);
s32 battle_unit_build_deployed_units_data(unit_deployment_coordinates_t* formation, s32 unused_1);
void battle_unit_init_deployed_units_data_for_debug_red_team(unit_deployment_coordinates_t* formation, s32 unused_1);
void battle_unit_store_entd_flags_without_control_flag(battle_unit_t* unit);

/* progression and pickups */
void battle_unit_apply_poach_morbol_transformation(battle_unit_t* unit);
u8 battle_unit_calculate_move_exp_jp_up_to_earn(const unit_stats_t* unit);
s32 battle_unit_count_crystals(void);
void battle_unit_find_war_trophies_and_bonus_money(battle_war_result_t* result);
battle_unit_t* battle_unit_get_crystal_or_treasure_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
void battle_unit_learn_used_ability(unit_stats_t* unit);
void battle_unit_transform_into_morbol(unit_stats_t* unit);
s32 battle_unit_apply_action_rewards(s32 unit_index, action_reward_display_t* display);
s32 battle_unit_build_action_rewards(unit_stats_t* unit, action_reward_display_t* rewards);
s32 battle_unit_learn_from_crystal(unit_stats_t* unit, s32 flags);
s32 battle_unit_load_job_level(unit_stats_t* unit, s32 job_id, s32* out_slot);
s32 battle_unit_add_job_jp(s32 unit_index, s32 jp, s32 team, s32 slot);
crystal_pickup_result_t* battle_unit_generate_crystal_or_treasure(unit_stats_t* mover);

#endif
