#ifndef BATTLE_UNIT_ENCOUNTER_H
#define BATTLE_UNIT_ENCOUNTER_H

/* BATTLE unit encounters: ENTD units, party deployment, and rewards, crystals, treasure and war trophies. */

#include "fft/battle_unit.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/menu_crystal_pickup.h"
#include "fft/unit.h"
#include "fft/unit_entd.h"
#include "psx/types.h"

/* encounters and deployment */
/* g_encounter_selection_mode: which ENTD unit_entd_select_and_open opens. */
typedef enum unit_entd_selection {
    UNIT_ENTD_SELECTION_SCRIPT_VARIABLE = 0, /* EVENT_SCRIPT_VAR_CURRENT_ENTD */
    UNIT_ENTD_SELECTION_ENTD_250 = 1,
    UNIT_ENTD_SELECTION_RANDOM_75_TO_79 = 2,
    UNIT_ENTD_SELECTION_RANDOM_1_TO_59 = 3,
    UNIT_ENTD_SELECTION_MAP_ID = 4, /* the map's own id when it is not 0 */
} unit_entd_selection_e;

extern s16 g_unit_entd_spritesheet_ids[];
extern s32 g_unit_entd_spritesheet_count;

unit_stats_t* unit_entd_find_stats(s32 unit_id, s32* out_battle_id);
void unit_entd_load(entd_encounter_t* entd_data, s32 mode);
s32 unit_put_on_field(u32 battle_id);
s32 deploy_init_party_member(s32 unused0, s32 unused1, s32 unit_index);
s32 unit_place_in_new_location(u32 runtime_id, battle_map_tile_coordinates_t* tile, s32 facing);
void reward_count_enemy_level(unit_stats_t* unit);
s32 unit_find_relocation_tile(s32 unit_index, battle_map_tile_coordinates_t* destination);
s32 deploy_build_units(unit_deployment_coordinates_t* formation, s32 unused_1);
void deploy_init_units_for_debug_red_team(unit_deployment_coordinates_t* formation, s32 unused_1);
void unit_store_team_flags(unit_t* unit);

/* progression and pickups */
/* crystal_apply_choice flags. */
enum {
    CRYSTAL_LEARN_ABILITIES = 0x1,
    CRYSTAL_RESTORE_HP_MP = 0x2,
    CRYSTAL_TAKE_TREASURE = 0x4, /* add the treasure's item instead */
};

/* War trophies and bonus money of the battle's units, filled by
 * reward_find_war_trophies_and_bonus_money; REQUIRE fills
 * g_war_trophy_search_result with it for the war trophy screen. */
typedef struct reward_war_result {
    u8 item_ids[UNIT_SLOT_COUNT]; /* 0x00 */
    u8 item_count;                /* 0x15 */
    u8 _padding_16[2];            /* 0x16; aligns bonus_money */
    s32 bonus_money;              /* 0x18; sum of bonus-money modifiers * 100 */
    s32 level_sum_money;          /* 0x1c; g_reward_enemy_level_sum * 100 */
    s32 level_bonus_money;        /* 0x20; (highest enemy level - lowest level) * 100 */
} reward_war_result_t;
typedef char reward_war_result_size_must_be_36[(sizeof(reward_war_result_t) == 36) ? 1 : -1];

void unit_apply_poach_morbol_transformation(unit_t* unit);
u8 reward_calculate_move_get_amount(const unit_stats_t* unit);
s32 unit_count_crystals(void);
void reward_find_war_trophies_and_bonus_money(reward_war_result_t* result);
unit_t* unit_get_crystal_or_treasure_at_map_coords(s32 map_x, s32 map_y, s32 map_z);
void ability_learn_used(unit_stats_t* unit);
void unit_transform_into_morbol(unit_stats_t* unit);
s32 reward_apply_action(s32 unit_index, action_reward_display_t* display);
s32 reward_build_display(unit_stats_t* unit, action_reward_display_t* rewards);
s32 crystal_apply_choice(unit_stats_t* unit, s32 flags);
s32 reward_load_job_level(unit_stats_t* unit, s32 job_id, s32* out_slot);
s32 reward_add_job_jp(s32 unit_index, s32 jp, s32 team, s32 slot);
crystal_pickup_result_t* crystal_build_pickup_result(unit_stats_t* mover);

#endif
