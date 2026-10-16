#ifndef MAIN_UNIT_H
#define MAIN_UNIT_H

/* Resident unit data: abilities, jobs, statuses, unit generation, the party, encounters and terrain costs. */

#include "fft/data.h"
#include "fft/data_ability.h"
#include "fft/data_job.h"
#include "fft/data_skillset.h"
#include "fft/menu.h"
#include "fft/menu_crystal_pickup.h"
#include "fft/unit.h"
#include "fft/unit_entd.h"
#include "fft/unit_party.h"
#include "psx/types.h"

/* abilities */
typedef struct main_ability_human_skillset {
    u8 high_id_bits[3];
    u8 ability_ids[HUMAN_SKILLSET_ABILITY_COUNT];
} main_ability_human_skillset_t;

typedef struct main_ability_monster_skillset {
    u8 high_id_bits;
    u8 ability_ids[MONSTER_SKILLSET_ABILITY_COUNT];
} main_ability_monster_skillset_t;

extern ability_data_t g_main_ability_data[];
extern ability_secondary_data_t g_main_ability_range_data[];
extern main_ability_human_skillset_t g_main_ability_human_skillset_abilities[];
extern main_ability_monster_skillset_t g_main_ability_monster_skillset_abilities[];
extern u8 g_main_action_menu_types_by_skillset[0xe0];
/* One secondary-data byte per ability from ABILITY_ID_MATH_FIRST on (math,
 * reaction, support, movement). */
extern u8 g_main_math_rsm_ability_data[ABILITY_ID_COUNT - ABILITY_ID_MATH_FIRST];
/* The item_type_e each Throw ability throws, indexed by ability id - ABILITY_ID_THROW_FIRST. */
extern u8 g_main_throw_ability_item_types[];
extern ability_jump_record_t g_main_jump_ability_data[12];
extern ability_charge_record_t g_main_charge_ability_data[8];
/* Indexed by reaction ability id - ABILITY_ID_REACTION_FIRST. */
extern const u8 g_main_reaction_behavior_flags[ABILITY_ID_SUPPORT_FIRST - ABILITY_ID_REACTION_FIRST];
extern u8 g_main_calculator_primes[CALCULATOR_PRIME_COUNT];

s32 main_ability_get_id_from_skillset(s32 skillset_id, s32 ability_index);
u16* main_ability_store_skillset_abilities(s32 skillset, s32 flags);
s32 main_ability_calculate_pointers_and_type(
    s32 id, ability_data_t** out_ability_data, ability_secondary_pointer_t* out_secondary_data);
u32 main_ability_get_known(const unit_stats_t* unit, u32 job_index);

/* jobs */
extern job_data_t g_main_job_data[];
extern job_data_t* g_main_job_data_pointer;
extern const u16 g_main_job_level_jp_requirements[UNIT_JOB_LEVEL_MAX];
extern u16 g_main_job_jp_requirements_by_level[];
extern const u8 g_main_job_unlock_requirements[UNIT_CAREER_JOB_COUNT - 1][UNIT_JOB_LEVEL_BYTE_COUNT];

job_data_t* main_job_get_data_pointer(s32 job_id);
s32 main_job_find_by_skillset(u8 skillset_id);
u32 main_job_calculate_level(u16 job_jp);
u32 main_job_calculate_unlocked(const u8* packed_job_levels, u32 gender_flags);
void main_job_store_unlock_bitset(u8 destination[UNIT_UNLOCKED_JOB_BYTE_COUNT], u32 unlocked_jobs);
s32 main_job_add_proposition_jp(u32 party_index, s32 jp);

/* statuses */
/* Values accepted by main_status_set_action_state. */
typedef enum main_unit_action_state {
    MAIN_UNIT_ACTION_STATE_NONE = 0,
    MAIN_UNIT_ACTION_STATE_CHARGING = 5,
    MAIN_UNIT_ACTION_STATE_JUMPING = 6,
    MAIN_UNIT_ACTION_STATE_DEFENDING = 7,
    /* The Performing status bit (0x01) shifted left 3, which KEEP_PERFORMING relies on. */
    MAIN_UNIT_ACTION_STATE_PERFORMING = 8,
    MAIN_UNIT_ACTION_STATE_KEEP_PERFORMING = 255
} main_unit_action_state_e;

/* How main_status_change_unit changes inflicted_status: OR, AND-not or
 * replacement. It then merges status_sets.innate into status_sets.current. */
typedef enum main_status_change {
    MAIN_STATUS_ADD = 0,
    MAIN_STATUS_REMOVE = 1,
    MAIN_STATUS_REPLACE = 2
} main_status_change_e;

/* Packed highest-order action status: a one-based index, or zero for none. */
enum {
    MAIN_ACTION_STATUS_RESULT_INDEX_MASK = 0x7f,
    MAIN_ACTION_STATUS_RESULT_REMOVAL = 0x80,
    MAIN_ACTION_STATUS_RESULT_SCRIPT_FILTER_EXCLUDED = 0x100,
};

extern status_infliction_data_t g_main_status_infliction_data[];
extern const status_effect_data_t g_main_status_effect_data[STATUS_COUNT];
/* Built status masks, indexed by status_check_set_e. */
extern u8 g_main_status_check_sets[STATUS_CHECK_SET_COUNT][STATUS_BYTE_COUNT];

void main_status_init_check_data(void);
void main_status_change_unit(unit_stats_t* unit, s32 status_byte_index, u8 status_flag, s32 change_type);
void main_status_store_current(unit_stats_t* unit);
void main_status_set_action_state(unit_stats_t* unit, u8 action_state);
s32 main_status_set_ct(unit_stats_t* unit, s32 status_id, s32 removing);
s32 main_status_find_action_highest_order_effect(const action_data_t* action);
s32 main_unit_has_status_in_set(const unit_stats_t* unit, status_check_set_e status_set);

/* unit generation */
/* Selects the rows of g_main_unit_generation_base_data and
 * g_main_unit_generation_raw_stat_variance; these are not job ids or gender
 * bit flags. */
typedef enum main_unit_generation_type {
    MAIN_UNIT_TYPE_MALE = 0,
    MAIN_UNIT_TYPE_FEMALE = 1,
    MAIN_UNIT_TYPE_RAMZA = 2,
    MAIN_UNIT_TYPE_MONSTER = 3,
    MAIN_UNIT_TYPE_COUNT = 4
} main_unit_generation_type_e;

/* One row of g_main_unit_generation_base_data, selected by
 * main_unit_generation_type_e. */
typedef struct main_unit_generation_base_data {
    u8 raw_stat_seeds[UNIT_RAW_STAT_COUNT];
    u8 equipment[UNIT_EQUIPMENT_SLOT_COUNT];
} main_unit_generation_base_data_t;
typedef char main_unit_generation_base_data_size_must_be_12[(sizeof(main_unit_generation_base_data_t) == 12) ? 1 : -1];

extern main_unit_generation_base_data_t g_main_unit_generation_base_data[MAIN_UNIT_TYPE_COUNT];
extern u8 g_main_unit_generation_raw_stat_variance[MAIN_UNIT_TYPE_COUNT][UNIT_RAW_STAT_COUNT];

/* Out-of-battle generation normalizes every value except male, female and
 * monster to Ramza. The raw-stat generator indexes the tables directly. */
s32 main_party_generate_unit(main_unit_generation_type_e unit_type);

/* units */
/* main_unit_init_from_entd kinds. */
typedef enum main_unit_entd_init_kind {
    MAIN_UNIT_ENTD_INIT_NEW = 0, /* reset the unit and build it from its ENTD entry */
    /* Rebuild a transformed unit's stats for its new job in place: no reset, no ENTD data and no
     * ENTD equipment. */
    MAIN_UNIT_ENTD_INIT_TRANSFORM = 0x82,
} main_unit_entd_init_kind_e;

s32 main_unit_init_from_entd(unit_stats_t* unit, entd_unit_t* entd, u32 use_world_text, s32 kind);
s32 main_unit_init_from_party_slot(unit_stats_t* unit, s32 slot, s32 use_world_text);
void main_unit_init_job_levels(const u16* unit_job_jp, u8* unit_job_levels);
void main_unit_init_status_and_rewards(unit_stats_t* unit, s32 clear_rewards);
void main_unit_init_position_and_rewards(unit_stats_t* unit, entd_unit_t* entd);
void main_unit_learn_job_abilities(unit_stats_t* unit, s32 job_id, const entd_unit_t* entd);
void main_unit_reset_battle_state(unit_stats_t* unit);
void main_unit_refresh_stats_and_statuses(unit_stats_t* unit);
void main_unit_update_formation_stats(unit_stats_t* unit);
void main_unit_set_equipment_attributes(unit_stats_t* unit, s32 level_up_check);
void main_unit_set_equipment_stats(unit_stats_t* unit);
void main_unit_set_equippable_items(unit_stats_t* unit);
s32 main_unit_calculate_random_equipment(
    unit_stats_t* unit, u8 item_type_flag, u8 weapon_flag_mask, u8 required_item_type);
void main_unit_calculate_move_jump(unit_stats_t* unit, s32 keep_hp_mp);
u8 main_unit_calculate_palette_spritesheet(unit_stats_t* unit, u8* palette);
u16 main_unit_calculate_rsm(unit_stats_t* unit, u16 ability_id, s32 flags, entd_unit_t* entd);
void main_unit_enable_rsm_flags(unit_stats_t* unit);
void main_unit_copy_last_ability_ct(unit_stats_t* unit);
s32 main_unit_check_level_up(unit_stats_t* unit);
void main_unit_apply_level_growth(unit_stats_t* unit, s32 level_down);
void main_unit_increase_casualty_counters(unit_stats_t* unit);

/* party */
extern party_unit_t g_main_party_units[PARTY_ROSTER_SLOT_COUNT];
extern u8 g_main_party_highest_level;

void main_party_clear_all(void);
void main_party_init_new_game(s32 mode);
party_unit_t* main_party_get_unit(s32 party_index);
s32 main_party_find_free_slot(s32 save_formation, u8* palette);
s32 main_party_save_unit(unit_stats_t* unit, s32 allow_guest);
s32 main_unit_save_entd_unit_to_party(entd_encounter_t* entd, s32 unit_id, s32 guest_id, s32 initialize_for_battle);
void main_party_remove_unit(u32 index);
s32 main_party_create_monster_egg(s32 monster_job, s32 egg_modifier, s32 egg_color);
u32 main_party_level_unit_to_target(party_unit_t* party_unit, s32 level_delta);
u8 main_party_calculate_highest_level(void);

/* encounters and deployment */
extern s32 g_main_entd_current_event_id;
extern entd_file_t* g_main_entd_set;
extern entd_unit_t* g_main_entd_current_unit;
extern s32 g_main_entd_selection_mode; /* battle_entd_selection_e */
extern unit_deployment_coordinates_t* g_main_unit_deployed_coords;
extern unit_deployment_coordinates_t g_main_deployed_unit_map_coordinates[6];

int main_entd_open_file(void);
void main_entd_init_event_unit_data(int event_id);
/* Current encounter, NULL while loading, or (entd_encounter_t *)-1 when
 * no encounter is selected. */
entd_encounter_t* main_entd_get_encounter(void);

/* crystals */
extern u8 g_main_crystal_learnable_abilities[];
extern crystal_pickup_result_t g_main_crystal_pickup_result;
extern u8 g_main_crystal_treasure_item_id;

/* zodiac and calendar */
extern const u16 g_main_zodiac_day_limits[ZODIAC_SIGN_ORDINARY_COUNT];
extern u16 g_main_month_start_day_offsets[13];

s32 main_unit_calculate_zodiac_symbol(u16 birthday);

/* portraits */
extern u8 g_main_special_portrait_unit_id;
/* WLDFACE portrait for portrait slot 20: ATTACK sets it from the scenario
 * record and attack_gfx_prepare_party_portrait_textures uploads it before the
 * battle; BATTLE never reads it. */
extern u8 g_main_special_portrait_wldface_id;

/* terrain */
/* g_main_terrain_status_flags has one byte per map_surface_type_e; only
 * MAP_SURFACE_POISONED_MARSH has a bit. */
enum {
    MAIN_TERRAIN_STATUS_FLAG_POISON = 0x80, /* battle_status_prepare_terrain_poison poisons a unit standing there */
};

/* Rows of g_main_terrain_movement_cost_tables, chosen from the unit's movement
 * abilities by battle_move_store_unit_movement_to_scratchpad; each has one
 * cost per map_surface_type_e. */
typedef enum main_terrain_cost_table {
    MAIN_TERRAIN_COST_TABLE_DEFAULT = 0,
    MAIN_TERRAIN_COST_TABLE_FLY_TELEPORT = 1, /* every surface costs 1 */
    MAIN_TERRAIN_COST_TABLE_FLOAT = 2,        /* every surface costs 1 */
    MAIN_TERRAIN_COST_TABLE_WATER = 3,        /* Move in Water and Walk on Water */
    MAIN_TERRAIN_COST_TABLE_UNDERWATER = 4,
    MAIN_TERRAIN_COST_TABLE_LAVA = 5,
} main_terrain_cost_table_e;

/* Costs in those rows that are not a number of Move points. */
enum {
    MAIN_TERRAIN_COST_WEATHER = 0,    /* replaced by battle_move_get_weather_terrain_cost */
    MAIN_TERRAIN_COST_BLOCKED = 0xff, /* stays so under Any Ground */
};

extern const u8 g_main_terrain_movement_cost_tables[][64];
extern const u8 g_main_terrain_status_flags[64];
extern const u8 g_main_terrain_geomancy_abilities[];
/* Two bytes per map_surface_type_e; all 64 pairs are 0x19, 0x19. */
extern u8 g_main_terrain_tile_info_pairs[64][2];

#endif
