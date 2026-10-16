#ifndef MAIN_UNIT_H
#define MAIN_UNIT_H

/* Resident unit data: generation, stats, jobs, abilities, statuses and the party. */

#include "fft/data.h"
#include "fft/data_ability.h"
#include "fft/data_job.h"
#include "fft/data_skillset.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/types.h"

typedef struct main_ability_human_skillset {
    u8 high_id_bits[3];
    u8 ability_ids[HUMAN_SKILLSET_ABILITY_COUNT];
} main_ability_human_skillset_t;

typedef struct main_ability_monster_skillset {
    u8 high_id_bits;
    u8 ability_ids[MONSTER_SKILLSET_ABILITY_COUNT];
} main_ability_monster_skillset_t;

/* Values accepted by main_status_set_action_state. */
typedef enum main_unit_action_state {
    MAIN_UNIT_ACTION_STATE_NONE = 0,
    MAIN_UNIT_ACTION_STATE_CHARGING = 5,
    MAIN_UNIT_ACTION_STATE_JUMPING = 6,
    MAIN_UNIT_ACTION_STATE_DEFENDING = 7,
    MAIN_UNIT_ACTION_STATE_PERFORMING = 8,
    MAIN_UNIT_ACTION_STATE_KEEP_PERFORMING = 255
} main_unit_action_state_e;

/* main_status_change_unit, 0x8005e6cc: OR, AND-not, or replacement of
 * inflicted_status, followed by merging status_sets.innate into status_sets.current. */
typedef enum main_status_change {
    MAIN_STATUS_ADD = 0,
    MAIN_STATUS_REMOVE = 1,
    MAIN_STATUS_REPLACE = 2
} main_status_change_e;

/* Selects rows in the base-data (0x8005e90c) and variance (0x8005e93c)
 * tables; these are not job IDs or gender bit flags. */
typedef enum main_unit_generation_type {
    MAIN_UNIT_TYPE_MALE = 0,
    MAIN_UNIT_TYPE_FEMALE = 1,
    MAIN_UNIT_TYPE_RAMZA = 2,
    MAIN_UNIT_TYPE_MONSTER = 3,
    MAIN_UNIT_TYPE_COUNT = 4
} main_unit_generation_type_e;

/* One row of the generation tables at 0x8005e90c and 0x8005e93c. The
 * records are selected by main_unit_generation_type_e, not by job id. */
typedef struct main_unit_generation_base_data {
    u8 raw_stat_seeds[UNIT_RAW_STAT_COUNT];
    u8 equipment[UNIT_EQUIPMENT_SLOT_COUNT];
} main_unit_generation_base_data_t;
typedef char main_unit_generation_base_data_size_must_be_12[(sizeof(main_unit_generation_base_data_t) == 12) ? 1 : -1];

/* ability */
extern ability_secondary_data_t g_main_ability_range_data[];
extern ability_data_t g_main_ability_data[];
extern main_ability_human_skillset_t g_main_ability_human_skillset_abilities[];
extern main_ability_monster_skillset_t g_main_ability_monster_skillset_abilities[];
s32 main_ability_get_id_from_skillset(s32 skillset_id, s32 ability_index);
u16* main_ability_store_skillset_abilities(s32 skillset, s32 flags);
s32 main_ability_calculate_pointers_and_type(
    s32 id, ability_data_t** out_ability_data, main_ability_secondary_pointer_t* out_secondary_data);
u32 main_ability_get_known(const unit_stats_t* unit, u32 job_index);

/* job */
extern job_data_t g_main_job_data[];
extern job_data_t* g_main_job_data_pointer;
extern const u16 g_main_job_level_jp_requirements[UNIT_JOB_LEVEL_MAX];
extern u16 g_main_job_jp_requirements_by_level[];
extern const u8 g_main_job_unlock_requirements[UNIT_CAREER_JOB_COUNT - 1][UNIT_JOB_LEVEL_BYTE_COUNT];
s32 main_job_add_proposition_jp(u32 party_index, s32 jp);
u32 main_job_calculate_level(u16 job_jp);
u32 main_job_calculate_unlocked(const u8* packed_job_levels, u32 gender_flags);
s32 main_job_find_by_skillset(u8 skillset_id);
job_data_t* main_job_get_data_pointer(s32 job_id);
void main_job_store_unlock_bitset(u8 destination[UNIT_UNLOCKED_JOB_BYTE_COUNT], u32 unlocked_jobs);

/* save */
extern u8* g_main_brave_story_ages_source;
extern u8 g_main_brave_story_character_ages[0x40];

/* status */
extern status_infliction_data_t g_main_status_infliction_data[];
extern const status_effect_data_t g_main_status_effect_data[STATUS_COUNT];
/* Built status masks, indexed by main_status_check_set_e. */
extern u8 g_main_status_check_sets[MAIN_STATUS_CHECK_SET_COUNT][STATUS_BYTE_COUNT];
void main_status_init_check_data(void);
void main_status_change_unit(unit_stats_t* unit, s32 status_byte_index, u8 status_flag, s32 change_type);
void main_status_set_action_state(unit_stats_t* unit, u8 action_state);
s32 main_status_set_ct(unit_stats_t* unit, s32 status_id, s32 removing);
void main_status_store_current(unit_stats_t* unit);
s32 main_status_find_action_highest_order_effect(const action_data_t* action);

/* unit */
extern u8 g_main_party_highest_level;
extern s32 g_main_entd_current_event_id;
extern entd_file_t* g_main_entd_set;
extern entd_unit_t* g_main_entd_current_unit;
extern party_unit_t g_main_party_units[PARTY_ROSTER_SLOT_COUNT];
extern main_unit_generation_base_data_t g_main_unit_generation_base_data[MAIN_UNIT_TYPE_COUNT];
extern u8 g_main_unit_generation_raw_stat_variance[MAIN_UNIT_TYPE_COUNT][UNIT_RAW_STAT_COUNT];
extern u32 g_main_unit_name_uses_world_text;
extern unit_deployment_coordinates_t* g_main_unit_deployed_coords;
extern u8 g_main_crystal_learnable_abilities[];
extern crystal_pickup_result_t g_main_crystal_pickup_result;
extern u8 g_main_crystal_treasure_item_id;
void main_unit_update_formation_stats(unit_stats_t* unit);
/* Current encounter, NULL while loading, or (entd_encounter_t *)-1 when
 * no encounter is selected. */
entd_encounter_t* main_entd_get_encounter(void);
int main_entd_open_file(void);
void main_entd_init_event_unit_data(int event_id);
void main_party_clear_all(void);
s32 main_party_create_monster_egg(s32 monster_job, s32 egg_modifier, s32 egg_color);
s32 main_party_find_free_slot(s32 save_formation, u8* palette);
/* Out-of-battle generation normalizes every value except male, female and
 * monster to Ramza. The raw-stat generator indexes the tables directly. */
s32 main_party_generate_unit(main_unit_generation_type_e unit_type);
party_unit_t* main_party_get_unit(s32 party_index);
void main_party_init_new_game(s32 mode);
u32 main_party_level_unit_to_target(party_unit_t* party_unit, s32 level_delta);
void main_party_remove_unit(u32 index);
s32 main_party_save_unit(unit_stats_t* unit, s32 allow_guest);
void main_unit_apply_level_growth(unit_stats_t* unit, s32 level_down);
void main_unit_calculate_move_jump(unit_stats_t* unit, s32 keep_hp_mp);
u8 main_unit_calculate_palette_spritesheet(unit_stats_t* unit, u8* palette);
s32 main_unit_calculate_random_equipment(
    unit_stats_t* unit, u8 item_type_flag, u8 weapon_flag_mask, u8 required_item_type);
u16 main_unit_calculate_rsm(unit_stats_t* unit, u16 ability_id, s32 flags, entd_unit_t* entd);
s32 main_unit_calculate_zodiac_symbol(u16 birthday);
s32 main_unit_check_level_up(unit_stats_t* unit);
void main_unit_copy_last_ability_ct(unit_stats_t* unit);
void main_unit_enable_rsm_flags(unit_stats_t* unit);
s32 main_unit_has_status_in_set(const unit_stats_t* unit, main_status_check_set_e status_set);
s32 main_unit_save_entd_unit_to_party(entd_encounter_t* entd, s32 unit_id, s32 guest_id, s32 initialize_for_battle);
s32 main_unit_init_from_entd(unit_stats_t* unit, entd_unit_t* entd, u32 use_world_text, s32 kind);
s32 main_unit_init_from_party_slot(unit_stats_t* unit, s32 slot, s32 use_world_text);
void main_unit_init_job_levels(const u16* unit_job_jp, u8* unit_job_levels);
void main_unit_init_status_and_rewards(unit_stats_t* unit, s32 clear_rewards);
void main_unit_learn_job_abilities(unit_stats_t* unit, s32 job_id, const entd_unit_t* entd);
void main_unit_refresh_stats_and_statuses(unit_stats_t* unit);
void main_unit_reset_battle_state(unit_stats_t* unit);
void main_unit_set_equipment_attributes(unit_stats_t* unit, s32 level_up_check);
/* SCUS_942.21 unit equipment/name pipeline, 0x8005ab00 and
 * 0x8005c398..0x8005cbd0. Signatures are shared with the matching definitions. */
void main_unit_set_equipment_stats(unit_stats_t* unit);
void main_unit_set_equippable_items(unit_stats_t* unit);
u8 main_party_calculate_highest_level(void);
void main_unit_increase_casualty_counters(unit_stats_t* unit);
void main_unit_init_position_and_rewards(unit_stats_t* unit, entd_unit_t* entd);

/* zodiac */
extern const u16 g_main_zodiac_day_limits[ZODIAC_SIGN_ORDINARY_COUNT];
extern POLY_FT4 g_main_zodiac_primitives[2][30];

/* gfx */
extern u8 g_main_special_portrait_unit_id;
/* WLDFACE portrait for portrait slot 20: ATTACK sets it from the scenario
 * record and attack_gfx_prepare_party_portrait_textures uploads it before the
 * battle; BATTLE never reads it. */
extern u8 g_main_special_portrait_wldface_id;

#endif
