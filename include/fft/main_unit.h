#ifndef FFT_MAIN_UNIT_H
#define FFT_MAIN_UNIT_H

#include "fft/battle.h"
#include "fft/data.h"
#include "fft/job.h"

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

typedef struct main_unit_generation_raw_stat_variance {
    u8 raw_stats[UNIT_RAW_STAT_COUNT];
} main_unit_generation_raw_stat_variance_t;

typedef char main_unit_generation_base_data_size_must_be_12[(sizeof(main_unit_generation_base_data_t) == 12) ? 1 : -1];
typedef char
    main_unit_generation_raw_stat_variance_size_must_be_5[(sizeof(main_unit_generation_raw_stat_variance_t) == 5) ? 1
                                                                                                                  : -1];

extern main_unit_generation_base_data_t g_main_unit_generation_base_data[MAIN_UNIT_TYPE_COUNT];
extern main_unit_generation_raw_stat_variance_t g_main_unit_generation_raw_stat_variance[MAIN_UNIT_TYPE_COUNT];

extern u32 g_main_unit_name_uses_world_text;

/* main_status_change_unit, 0x8005e6cc: OR, AND-not, or replacement of
 * inflicted_status, followed by merging status_sets.innate into status_sets.current. */
typedef enum main_status_change {
    MAIN_STATUS_ADD = 0,
    MAIN_STATUS_REMOVE = 1,
    MAIN_STATUS_REPLACE = 2
} main_status_change_e;

/* Values accepted by main_status_set_action_state. */
typedef enum main_unit_action_state {
    MAIN_UNIT_ACTION_STATE_NONE = 0,
    MAIN_UNIT_ACTION_STATE_CHARGING = 5,
    MAIN_UNIT_ACTION_STATE_JUMPING = 6,
    MAIN_UNIT_ACTION_STATE_DEFENDING = 7,
    MAIN_UNIT_ACTION_STATE_PERFORMING = 8,
    MAIN_UNIT_ACTION_STATE_KEEP_PERFORMING = 0xff
} main_unit_action_state_e;

/* ability */
s32 main_ability_get_id_from_skillset(s32 skillset_id, s32 ability_index);
u16* main_ability_store_skillset_abilities(s32 skillset, s32 flags);

/* entd */
void main_entd_init_event_unit_data(int event_id);

/* item */
item_data_t* main_item_get_data_pointer(s32 item_id);

/* job */
u32 main_job_calculate_level(u16 job_jp);
u32 main_job_calculate_unlocked(const u8* packed_job_levels, u32 gender_flags);
job_data_t* main_job_get_data_pointer(s32 job_id);
s32 main_job_get_random_unlocked(const battle_stats_t* unit);
void main_job_store_unlock_bitset(u8 destination[UNIT_UNLOCKED_JOB_BYTE_COUNT], u32 unlocked_jobs);

/* party */
void main_party_clear_all(void);
s32 main_party_create_monster_egg(s32 monster_job, s32 egg_modifier, s32 egg_color);
s32 main_party_find_free_slot(s32 save_formation, u8* palette);
s32 main_party_find_slot_by_sprite_set(s32 sprite_set);

/* Out-of-battle generation normalizes every value except male, female and
 * monster to Ramza. The raw-stat generator indexes the tables directly. */
s32 main_party_generate_unit(s32 unit_type);
party_data_t* main_party_get_data_pointer(s32 party_index);
void main_party_init_new_game(s32 mode);
u32 main_party_level_unit_to_target(party_data_t* party_unit, s32 level_delta);
void main_party_remove_unit(u32 index);
s32 main_party_save_unit(battle_stats_t* unit, s32 allow_guest);

/* status */
void main_status_change_unit(battle_stats_t* unit, s32 status_set, u8 status_flag, s32 change_type);
void main_status_init_ct(battle_stats_t* unit);
void main_status_init_unit(battle_stats_t* unit);
void main_status_set_action_state(battle_stats_t* unit, u8 action_state);
s32 main_status_set_ct(battle_stats_t* unit, s32 status_id, s32 removing);
void main_status_store_current(battle_stats_t* unit);
void main_status_update_unit_flags_and_ct(battle_stats_t* unit);

/* unit */
void main_unit_apply_equipment_move_jump_and_name(battle_stats_t* unit);
void main_unit_apply_level_growth(battle_stats_t* unit, s32 level_down);
void main_unit_calculate_abilities(battle_stats_t* unit, entd_unit_t* entd);
void main_unit_calculate_actual_stats(battle_stats_t* unit, s32 mode);
s32 main_unit_calculate_entd_data(battle_stats_t* unit, entd_unit_t* entd);
void main_unit_calculate_entd_equipment(battle_stats_t* unit, entd_unit_t* entd);
void main_unit_calculate_jobs_and_skillsets_from_entd(battle_stats_t* unit, const entd_unit_t* entd);
void main_unit_calculate_move_jump(battle_stats_t* unit, s32 keep_position);
u8 main_unit_calculate_palette_spritesheet(battle_stats_t* unit, u8* palette);
s32 main_unit_calculate_random_equipment(
    battle_stats_t* unit, u8 item_type_flag, u8 weapon_flag_mask, u8 required_item_type);
u16 main_unit_calculate_rsm(battle_stats_t* unit, u16 ability_id, s32 flags, entd_unit_t* entd);
s32 main_unit_calculate_zodiac_symbol(u32 birthday);
s32 main_unit_check_level_up(battle_stats_t* unit);
void main_unit_copy_job_data(battle_stats_t* unit);
void main_unit_copy_job_growths_and_multipliers(battle_stats_t* unit);
void main_unit_copy_last_ability_ct(battle_stats_t* unit);
void main_unit_enable_rsm_flags(battle_stats_t* unit);
void main_unit_generate_base_raw_stats(u8* raw_stats, s32 unit_type);
void main_unit_generate_out_of_battle(party_data_t* party, s32 unit_type);
void main_unit_generate_party_base_raw_stats(party_data_t* party, s32 unit_type);
void main_unit_generate_raw_stats(battle_stats_t* unit);
u8 main_unit_get_spritesheet_palette(battle_stats_t* unit, u8* palette);
s32 main_unit_has_status_in_set(const battle_stats_t* unit, main_status_check_set_e status_set);
s32 main_unit_init(entd_encounter_t* entd, s32 unit_id, s32 guest_id, s32 initialize_for_battle);
void main_unit_init_battle_data(battle_stats_t* unit, const party_data_t* party_unit);
s32 main_unit_init_for_battle(battle_stats_t* unit, entd_unit_t* entd, u32 use_world_text, s32 kind);
s32 main_unit_init_job_data(battle_stats_t* unit, s32 slot, s32 use_world_text);
s32 main_unit_init_job_data_from_entd(battle_stats_t* unit, entd_unit_t* entd);
void main_unit_init_job_levels(u16* unit_job_jp, u8* unit_job_levels);
void main_unit_init_status_and_rewards(battle_stats_t* unit, s32 clear_rewards);
void main_unit_learn_job_abilities(battle_stats_t* unit, s32 job_id, const entd_unit_t* entd);
void main_unit_refresh_stats_and_statuses(battle_stats_t* unit);
void main_unit_reset_battle_state(battle_stats_t* unit);
void main_unit_set_equipment_attributes(battle_stats_t* unit, s32 level_up_check);

/* SCUS_942.21 unit equipment/name pipeline, 0x8005ab00 and
 * 0x8005c398..0x8005cbd0. Signatures are shared with the matching definitions. */
void main_unit_set_equipment_stats(battle_stats_t* unit);
void main_unit_set_equippable_items(battle_stats_t* unit);
void main_unit_set_rsm_flag(battle_stats_t* unit, u32 ability_id);
void main_unit_store_character_names(battle_stats_t* unit);
void main_unit_store_monster_equipment(battle_stats_t* unit, const entd_unit_t* entd);
void main_unit_store_ramza_name_birthday_zodiac(battle_stats_t* unit);
void main_unit_update_stats_statuses_and_equipment(battle_stats_t* unit, s32 skip_status_check, s32 initializing);

/* util */
void main_util_copy_bytes(const void* source, void* destination, int count);

s32 main_item_get_total_equipment_quantity(u8 item_id, s32 include_battle);

s32 main_ability_calculate_pointers_and_type(s32 id, u8** out_ability_data, u8** out_secondary_data);
u32 main_ability_get_known(battle_stats_t* unit, u32 skillset_id);
u8 main_party_calculate_highest_level(void);
s32 main_status_find_action_highest_order_effect(const u8* action);
void main_unit_increase_casualty_counters(battle_stats_t* unit);
void main_unit_init_position_and_rewards(battle_stats_t* unit, entd_unit_t* entd);
void save_unit_to_party(battle_stats_t* unit, u32 save_formation);

#endif
