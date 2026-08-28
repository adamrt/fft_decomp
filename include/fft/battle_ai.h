#ifndef FFT_BATTLE_AI_H
#define FFT_BATTLE_AI_H

#include "fft/battle.h"

/* Runtime policy byte in battle_ai_ability_entry_t. The low three bits control
 * how the AI searches and consumes candidate action origins; initialization
 * recomputes them while preserving the remaining bits. */
typedef enum battle_ai_ability_entry_flag {
    BATTLE_AI_ABILITY_ENTRY_CONSUME_EVALUATED_ORIGINS = 0x01,
    BATTLE_AI_ABILITY_ENTRY_FORCE_CONSUME_EVALUATED_ORIGINS = 0x02,
    BATTLE_AI_ABILITY_ENTRY_EXHAUSTIVE_ORIGIN_SCAN = 0x04,
    BATTLE_AI_ABILITY_ENTRY_REQUIRES_MONSTER_SKILL = 0x08,
    BATTLE_AI_ABILITY_ENTRY_ENEMIES_ONLY = 0x20,
    BATTLE_AI_ABILITY_ENTRY_ALLIES_ONLY = 0x40,
    BATTLE_AI_ABILITY_ENTRY_USABLE = 0x80,
    BATTLE_AI_ABILITY_ENTRY_INIT_PRESERVE_MASK = 0xf8,
} battle_ai_ability_entry_flag_e;

typedef enum battle_ai_rank_order {
    BATTLE_AI_RANK_BELOW = 0,
    BATTLE_AI_RANK_EQUAL = 1,
    BATTLE_AI_RANK_ABOVE = 2,
} battle_ai_rank_order_e;

typedef enum battle_ai_unit_filter {
    BATTLE_AI_UNIT_FILTER_ANY = 0,
    BATTLE_AI_UNIT_FILTER_ENEMIES = 1,
    BATTLE_AI_UNIT_FILTER_ALLIES = 2,
    BATTLE_AI_UNIT_FILTER_SPECIFIC = 3,
    BATTLE_AI_UNIT_FILTER_STATUS = 4,
} battle_ai_unit_filter_e;

typedef enum battle_ai_nearest_target_mode {
    BATTLE_AI_NEAREST_WITHOUT_BLOOD_SUCK = 0,
    BATTLE_AI_NEAREST_IMMINENT_ENEMY = 1,
    BATTLE_AI_NEAREST_ENEMY = 2,
    BATTLE_AI_NEAREST_UNIT = 3,
    BATTLE_AI_NEAREST_ELIGIBLE_ENEMY = 4,
    BATTLE_AI_NEAREST_HEALER_OR_CRYSTAL = 5,
} battle_ai_nearest_target_mode_e;

typedef enum battle_ai_destination_order {
    BATTLE_AI_DESTINATION_PRIORITY_FIRST = 0,
    BATTLE_AI_DESTINATION_DISTANCE_FIRST = 1,
} battle_ai_destination_order_e;

typedef enum battle_ai_tile_mask_mode {
    BATTLE_AI_TILE_MASK_RANGE = 0,
    BATTLE_AI_TILE_MASK_AOE = 1,
    BATTLE_AI_TILE_MASK_RANGE_PLUS_AOE = 2,
    BATTLE_AI_TILE_MASK_EXPLICIT = 3,
} battle_ai_tile_mask_mode_e;

typedef enum battle_ai_autobattle_mode {
    BATTLE_AI_AUTOBATTLE_TARGETED = 0x0C,
    BATTLE_AI_AUTOBATTLE_RECOVERY = 0x0E,
    BATTLE_AI_AUTOBATTLE_FADING_LIFE = 0x10,
    BATTLE_AI_AUTOBATTLE_RETREAT = 0x11,
} battle_ai_autobattle_mode_e;

typedef enum battle_ai_command_build_stage {
    BATTLE_AI_COMMAND_BUILD_REFRESH = 0,
    BATTLE_AI_COMMAND_BUILD_MOVEMENT = 1,
    BATTLE_AI_COMMAND_BUILD_CHOOSE_ACTION = 2,
} battle_ai_command_build_stage_e;

typedef enum battle_ai_direction {
    BATTLE_AI_DIRECTION_SOUTH = 0,
    BATTLE_AI_DIRECTION_WEST = 1,
    BATTLE_AI_DIRECTION_NORTH = 2,
    BATTLE_AI_DIRECTION_EAST = 3,
    BATTLE_AI_DIRECTION_OVERLAP = 4,
    BATTLE_AI_DIRECTION_CHOOSE_AT_WAIT = 5,
} battle_ai_direction_e;

typedef enum battle_ai_targeting_flags_1 {
    BATTLE_AI_TARGETING_FLAG_1_REFLECTED_ACTION = 0x08,
} battle_ai_targeting_flags_1_e;

/* Classification from the acting unit's side, not the sign of HP damage.
 * A useful effect may include deliberately hitting an ally to affect status. */
typedef enum battle_ai_ability_effect {
    BATTLE_AI_ABILITY_EFFECT_NONE = 0,
    BATTLE_AI_ABILITY_EFFECT_USEFUL = 1,
    BATTLE_AI_ABILITY_EFFECT_ADVERSE = 2,
} battle_ai_ability_effect_e;

/* Signed comparison view of an action's four-byte tail at +0x1c.
 * Callers pass &action->rank_byte; the stored priority's unsigned view
 * elsewhere must not change these signed comparisons. */
typedef struct battle_ai_action_rank {
    u8 rank_byte;
    u8 base_hit_percent;
    s16 priority;
} battle_ai_action_rank_t;

typedef char battle_ai_action_rank_size_must_be_4[(sizeof(battle_ai_action_rank_t) == 4) ? 1 : -1];

/* Cardinal map-tile deltas stored as wrapping unsigned bytes. */
typedef union battle_ai_facing_tile_offsets {
    u8 bytes[8];
    u8 by_direction[4][2];
} battle_ai_facing_tile_offsets_t;

/* 0x80193d70: copied to the stack at 0x801984ec, then read as unsigned
 * status IDs. Signed byte fields retain the target's three-byte copy. */
enum {
    BATTLE_AI_RECOVERY_STATUS_COUNT = 3,
};

typedef struct battle_ai_recovery_status_list {
    s8 ids[BATTLE_AI_RECOVERY_STATUS_COUNT];
} battle_ai_recovery_status_list_t;

typedef char battle_ai_recovery_status_list_must_be_3
    [sizeof(battle_ai_recovery_status_list_t) == BATTLE_AI_RECOVERY_STATUS_COUNT ? 1 : -1];

extern battle_ai_unit_decision_t* g_battle_ai_acting_unit_decision_ptr; /* data.acting_unit_decision */
extern u8 g_battle_ai_acting_unit_id;                                   /* 0x801a01f2; data.acting_unit_id */
extern u8 g_battle_ai_acting_unit_remaining_clockticks;
extern battle_ai_command_t g_battle_ai_command_history[2];             /* 0x801a0d94, 0x801a0dac */
extern battle_ai_ability_flags_t g_battle_ai_considered_ability_flags; /* data.considered_ability.ai_flags */
extern u8 g_battle_ai_considered_ability_flags_1;
extern battle_ai_coords_t g_battle_ai_considered_coords; /* 0x801a0038; data.considered_unit_coords */
extern s32 g_battle_ai_cowardly_movement_step;
extern u8 g_battle_ai_current_ability_data_backup[];
extern u8 g_battle_ai_current_action_base_hit_percent;
extern u8 g_battle_ai_current_action_rank_byte; /* 0x8019f3f4; data.current_action.rank_byte */

/* One shared AI workspace at 0x8019f3c4, not a separate record per candidate.
 * The byte symbols below alias fields of this object; they allocate no
 * additional storage. Preserve resume-state writes before returning -1. */
extern battle_ai_data_t g_battle_ai_data_base;
extern battle_ai_facing_tile_offsets_t g_battle_ai_facing_tile_offsets; /* 0x8019f358 */
extern u8 g_battle_ai_hit_counter;                                      /* data.hit_counter */
extern s32 g_battle_ai_math_ability_index;
extern u16* g_battle_ai_math_ability_list;
extern s32 g_battle_ai_math_skillset;
extern s32 g_battle_ai_math_skillset_index;
extern u8 g_battle_ai_movement_scenario;        /* data.movement_scenario */
extern u8 g_battle_ai_outcome_evaluation_state; /* 0x801a0034; data.outcome_evaluation_state */
extern battle_ai_recovery_status_list_t g_battle_ai_recovery_status_ids;
extern s32 g_battle_ai_recovery_status_index; /* 0x8019f380; status-list cursor preserved for resume */
extern s32 g_battle_ai_recovery_status_retry; /* 0x8019f37c; skip status pass after one failed attempt */
extern s32 g_battle_ai_retreat_phase;         /* 0x8019f390; retreat child resume stage */

/* 0x8019f384–0x8019f38c; cancellation-search accounting survives suspension. */
extern s32 g_battle_ai_status_candidate_count;
extern s32 g_battle_ai_status_insufficient_mp;

/* Signed weights for status IDs 0..39, read by 0x8019d864–0x8019d988. */
extern s16 g_battle_ai_status_priority_weights[40]; /* 0x8019f308..0x8019f357 */
extern s32 g_battle_ai_status_sufficient_mp;
extern battle_stats_t* g_battle_ai_temp_unit_data;
extern u8 g_battle_ai_unit_crystal_treasure_status; /* data.crystal_treasure_status */
extern u8 g_battle_ai_unit_enemy_flag;

/* Shares the 0x80193924 scratchpad-save arena used during movement propagation. */
extern battle_ai_extended_snapshot_t g_battle_ai_unit_snapshot_storage[];
extern battle_ai_extended_snapshot_t* g_battle_ai_unit_snapshots; /* 0x8019f3c0 */
extern battle_ai_data_t* g_battle_ai_workspace;                   /* 0x8019f3ac */

/* Typed alias of targeting_panel_data: 512 map panels followed by 16 auxiliary
 * panels, all initialized at 0x80174e84. This declaration allocates no storage. */
extern battle_target_panel_t g_battle_target_panels[];
extern u16 g_battle_team_golem[4]; /* 0x8018f5f4 */

/* Unnamed data, in address order. */

/* action */
s32 battle_action_init_current_data(s32 unit_id);
void battle_action_store_acting_unit_data(battle_stats_t* unit);
s32 battle_action_store_target_stats_pointer_data(s32 unit_id);

/* ai */
s32 battle_ai_build_command(s32 unit_id, battle_ai_command_t* command);
void battle_ai_build_monster_skill_tile_mask(void);
s32 battle_ai_build_movement_scenarios(void);
void battle_ai_build_targetable_tile_mask(battle_ai_tile_mask_mode_e mode, s32 radius);
s32 battle_ai_build_targeted_movement(void);
void battle_ai_build_unit_ability_list(s32 unit_id);
s32 battle_ai_calculate_ability_range(void);
s32 battle_ai_calculate_clockticks_until_death_counter_expires();
s32 battle_ai_calculate_clockticks_until_unit_acts(battle_stats_t* unit);
s32 battle_ai_calculate_distance_between_units(battle_stats_t* a, battle_stats_t* b);
s32 battle_ai_calculate_ratio_times_4(s32 divisor, s32 dividend);
s32 battle_ai_calculate_height_difference_between_units(battle_ai_coords_t* coords, battle_stats_t* unit);
s32 battle_ai_call_ability_processing(battle_ai_command_action_t* action);
s32 battle_ai_can_elemental_ability_be_used(battle_ai_coords_t* coords);
s32 battle_ai_should_exclude_from_imminent_enemies(s32 unit_id);
s32 battle_ai_can_unit_be_targeted_cryst_trea_mount_trans(s32 unit_id);
s32 battle_ai_check_ability_use_at_coords(battle_ai_coords_t* coordinates);
s32 battle_ai_check_ability_use_based_on_ct(s32 unit_index);
s32 battle_ai_check_facing_tile(battle_ai_coords_t* source, battle_ai_coords_t* target);
s32 battle_ai_can_cancel_current_status(battle_stats_t* unit, s32 status_bit);
s32 battle_ai_is_status_active_through_delay(s32 delay, battle_stats_t* unit, s32 status_id);
s32 battle_ai_check_map_allows_use_and_find_highest_target(void);
s32 battle_ai_check_set_highest_unit_priority(void);
s32 battle_ai_check_target_type(s32 unit_id);
void battle_ai_check_unit_for_crystal_or_treasure_status(void);
s32 battle_ai_check_unit_gets_turn_without_status(battle_stats_t* unit, s32 status);
s32 battle_ai_choose_move_from_move_list(void);
s32 battle_ai_choose_random_action(void);
s32 battle_ai_choose_status_action(void);
s32 battle_ai_choose_wait_facing(void);
battle_ai_ability_effect_e battle_ai_classify_ability_effect(s32 unit_id, s32 check_reflect);
void battle_ai_clear_current_action_rank_byte(void);
battle_ai_rank_order_e battle_ai_compare_target_priority_and_hit_rate(
    battle_ai_action_rank_t* a, battle_ai_action_rank_t* b);
s32 battle_ai_count_flagged_tiles_within_map_bounds(u16* rows);
void battle_ai_disable_ability_if_only_inflicts_status(battle_ai_ability_entry_t* entry, s32 status_id);
s32 battle_ai_evaluate_ability_outcome(void);
s32 battle_ai_evaluate_charging_movement(void);
s32 battle_ai_evaluate_math_targets(void);
s32 battle_ai_evaluate_movement_origins(void);
s32 battle_ai_evaluate_reflected_target_origins(void);
s32 battle_ai_evaluate_self_target_origins(void);
s32 battle_ai_evaluate_status_cancellation(s32 unit_id, s32 status_id);
s32 battle_ai_evaluate_target_tiles(void);
void battle_ai_fill_foe_proximity_grid(void);
s32 battle_ai_fill_target_distance_grid(battle_ai_coords_t* target);
s32 battle_ai_filter_reachable_tiles_by_target_distance(s32 distance_limit);
battle_ai_direction_e battle_ai_find_direction_of_target(const s8* target, const s8* origin);
s32 battle_ai_find_nearest_target(battle_ai_nearest_target_mode_e mode);
s32 battle_ai_find_unit_at_coordinates(battle_ai_coords_t* coords);
s32 battle_ai_get_movement_range(void);
s32 battle_ai_handle_autobattle(void);
s32 battle_ai_has_any_unit_decided_to_use_ability(void);
s32 battle_ai_has_any_unit_on_target_panel(void);
void battle_ai_init_acting_unit_data(void);
void battle_ai_init_selected_action(void);
void battle_ai_init_target_consideration(void);
void battle_ai_init_unit_abilities(s32 unit_id);
void battle_ai_init_workspace(void);
void battle_ai_insert_ranked_action(void);
void battle_ai_invert_target_priority(void);
s32 battle_ai_is_action_higher_ranked(battle_ai_action_data_t* a, battle_ai_action_data_t* b);
s32 battle_ai_is_vsync_hblank_past_threshold(void);
void battle_ai_load_ability_entry(battle_ai_ability_entry_t* entry);
s32 battle_ai_load_known_ability_flag(s32 unit_id, s32 skillset_id, s32 bit);
s32 battle_ai_propagate_target_movement(
    s32 unit_id, s32 target_x, s32 target_y, s32 target_elevation, s32 initialize, s32* suspended, s32 check_budget);
void battle_ai_record_considered_coords(s32 x, s32 y, s32 level);
s32 battle_ai_refresh_unit_decision_flags(void);
void battle_ai_reset_action_ranking_fields(battle_ai_action_data_t* action);
void battle_ai_restore_considered_action_data(void);
void battle_ai_restore_unit_state(s32 mode);
s32 battle_ai_run_cowardly_movement(void);
s32 battle_ai_run_non_specific_autobattle(void);
void battle_ai_move_temp_unit_to_coords(battle_ai_coords_t* coordinates);
s32 battle_ai_save_fading_life_auto_battle(void);
void battle_ai_save_unit_state(s32 mode);
s32 battle_ai_score_reachable_tiles(void);
s32 battle_ai_select_active_units(battle_ai_unit_filter_e mode, s32 target_id);
void battle_ai_select_candidate_coords_and_check_crystal(void);
s32 battle_ai_select_destination(s32 max_distance, battle_ai_destination_order_e order);
void battle_ai_select_destination_with_nearest_fallback(s32 range);
void battle_ai_select_destination_with_range_floor(s32 range);
s32 battle_ai_select_initial_action(void);
s32 battle_ai_select_peril_target(void);
s32 battle_ai_select_ranked_action(void);
s32 battle_ai_select_recovery_action(void);
s32 battle_ai_select_retreat_action(void);
s32 battle_ai_select_targeted_action(void);
s32 battle_ai_simulate_action_and_score(void);
void battle_ai_set_candidate_and_actor_reachable_tiles(void);
s32 battle_ai_exclude_enemies_acting_before_ct(void);
void battle_ai_simulate_movement_and_pickup(void);
void battle_ai_simulate_strikes(battle_ai_command_action_t* action, s32 reaction_mode);
void battle_ai_store_considered_action_data(void);
void battle_ai_store_main_target_id_and_focus_on_target_flag(s32 unit_id);
void battle_ai_store_weapon_attack_data(battle_ai_weapon_data_t* out, s32 item_id, s32* flags_out);
void battle_ai_take_next_movement_origin(void);
s32 battle_ai_take_random_tile(u16* tiles, battle_ai_coords_t* coordinates, s32 count);
void battle_ai_transfer_ability_data_and_set_defend_flag(void);
void battle_ai_transfer_byte_values(u8* destination, const u8* source, s32 byte_count);
void battle_ai_transfer_halfword_values(u16* destination, u16* source, s32 byte_count);
void battle_ai_transfer_unit_coordinates(s32 unit_index, battle_ai_coords_t* destination);
void battle_ai_update_team_targeting_flags(void);

s32 battle_ai_add_unique_value_to_list(s32 mode, s16 value, s32 limit);

s32 battle_ai_add_usable_skillset_abilities(s32 unit_id, s32 skillset, s32 count);

#endif
