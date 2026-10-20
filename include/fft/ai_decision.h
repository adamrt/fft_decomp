#ifndef AI_DECISION_H
#define AI_DECISION_H

/* How the battle AI decides a turn: action selection, targets, movement, ability evaluation, ranking, simulation and
 * facing. Each pass may suspend on its frame budget and resume next frame from the stage it saved in the workspace. */

#include "fft/action.h"
#include "fft/ai.h"
#include "fft/battle_map.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "psx/types.h"

/* frame budget */
/* Scanlines since vertical blank at which each search yields. */
enum { AI_TURN_SIMULATION_SCANLINE_LIMIT = 313, AI_PATHFIND_SCANLINE_LIMIT = 325, AI_DECISION_SCANLINE_LIMIT = 441 };

s32 ai_pathfinding_budget_is_exhausted(void);

/* action selection */
/* Byte-sized resume labels for initial action selection; the gaps are not dispatched. */
typedef enum ai_initial_selection_stage {
    AI_INITIAL_SELECTION_STATUS = 0,
    AI_INITIAL_SELECTION_RETREAT = 2,
    AI_INITIAL_SELECTION_AUTOBATTLE = 3,
    AI_INITIAL_SELECTION_GENERAL = 7
} ai_initial_selection_stage_e;

typedef enum ai_status_selection_stage {
    AI_STATUS_SELECTION_CHICKEN = 0,
    AI_STATUS_SELECTION_TARGET_DISTANCE = 1
} ai_status_selection_stage_e;

/* These three suspend points use their own byte; other values resume abilities. */
typedef enum ai_general_stage {
    AI_GENERAL_CHARGING = 2,
    AI_GENERAL_MOVEMENT = 4,
    AI_GENERAL_ABILITIES = 7
} ai_general_stage_e;

typedef enum ai_fight_for_life_stage {
    AI_FIGHT_FOR_LIFE_SELECTION_SCORE_TILES = 0,
    AI_FIGHT_FOR_LIFE_SELECTION_MOVEMENT = 1,
    AI_FIGHT_FOR_LIFE_SELECTION_CHARGING = 2,
    AI_FIGHT_FOR_LIFE_SELECTION_TARGET_ABILITIES = 3,
    AI_FIGHT_FOR_LIFE_SELECTION_OTHER_ABILITIES = 4
} ai_fight_for_life_stage_e;

/* No pass saves stage 5; it follows the same ally-search fallback as stage 6. */
typedef enum ai_protect_allies_stage {
    AI_PROTECT_ALLIES_SELECTION_SCORE_TILES = 0,
    AI_PROTECT_ALLIES_SELECTION_MOVEMENT = 1,
    AI_PROTECT_ALLIES_SELECTION_CHARGING = 2,
    AI_PROTECT_ALLIES_SELECTION_TARGET_ABILITIES = 3,
    AI_PROTECT_ALLIES_SELECTION_ENEMY_ABILITIES = 4,
    AI_PROTECT_ALLIES_SELECTION_ALLY_ABILITIES = 6,
    AI_PROTECT_ALLIES_SELECTION_CANCEL_STATUS = 7
} ai_protect_allies_stage_e;

typedef enum ai_retreat_stage {
    AI_RETREAT_MOVEMENT = 0,
    AI_RETREAT_SELF_ABILITIES = 1,
    AI_RETREAT_CHARGING = 2,
    AI_RETREAT_OTHER_ABILITIES = 3
} ai_retreat_stage_e;

enum { AI_PROTECT_ALLIES_STATUS_COUNT = 3 };

/* target selection */
typedef enum ai_nearest_target_mode {
    AI_NEAREST_WITHOUT_BLOOD_SUCK = 0,
    AI_NEAREST_IMMINENT_ENEMY = 1,
    AI_NEAREST_ENEMY = 2,
    AI_NEAREST_UNIT = 3,
    AI_NEAREST_ELIGIBLE_ENEMY = 4,
    AI_NEAREST_HEALER_OR_CRYSTAL = 5
} ai_nearest_target_mode_e;

/* Classification from the acting unit's side, not the sign of HP damage.
 * A useful effect may include deliberately hitting an ally to affect status. */
typedef enum ai_ability_effect {
    AI_ABILITY_EFFECT_NONE = 0,
    AI_ABILITY_EFFECT_USEFUL = 1,
    AI_ABILITY_EFFECT_ADVERSE = 2
} ai_ability_effect_e;

ai_ability_effect_e ai_target_classify_ability_effect(s32 unit_id, s32 check_reflect);
s32 ai_target_find_nearest(ai_nearest_target_mode_e mode);
s32 ai_tile_unit_find(const ai_coords_t* coords);
s32 ai_target_is_untargetable(s32 unit_id);

/* status and turn timing */
s32 ai_unit_turn_delay_calculate(const unit_stats_t* unit);
s32 ai_status_is_active_through_delay(s32 delay, const unit_stats_t* unit, s32 status_id);

/* Declared without parameters: callers pass only the unit, and the definition takes two more
 * that it overwrites before use. */
s32 ai_unit_death_delay_calculate();

/* movement */
typedef enum ai_movement_stage { AI_MOVEMENT_TARGET_DISTANCE = 0, AI_MOVEMENT_SCORE_TILES = 1 } ai_movement_stage_e;

typedef enum ai_move_retreat_stage {
    AI_MOVE_RETREAT_SCORE_TILES = 0,
    AI_MOVE_RETREAT_TARGET_DISTANCE = 1
} ai_move_retreat_stage_e;

s32 ai_move_propagate_from_target(
    s32 unit_id, s32 target_x, s32 target_y, s32 target_elevation, s32 initialize, s32* suspended, s32 check_budget);
void ai_triggered_trap_coords_store(s32 x, s32 y, s32 level);

/* target geometry */
void ai_cast_aim_tiles_build(void);
s32 ai_unit_distance_calculate(const unit_stats_t* a, const unit_stats_t* b);
s32 ai_height_difference_calculate(const ai_coords_t* coords, const unit_stats_t* unit);
void ai_unit_coordinates_copy(s32 unit_index, ai_coords_t* destination);
s32 ai_flagged_tiles_count(u16 rows[BATTLE_MAP_TERRAIN_LAYER_COUNT][AI_GRID_ROW_COUNT]);
s32 ai_origin_is_allowed(const ai_coords_t* coordinates);

/* ability evaluation */
typedef enum ai_ability_evaluation_stage {
    AI_ABILITY_EVALUATION_REFLECTED_TARGETS = 0,
    AI_ABILITY_EVALUATION_SELF_TARGET_RANGE = 1,
    AI_ABILITY_EVALUATION_UNIT_TARGETS = 2,
    AI_ABILITY_EVALUATION_FIRST_CT_GROUP = 3,
    AI_ABILITY_EVALUATION_SECOND_CT_GROUP = 4,
    AI_ABILITY_EVALUATION_TILE_TARGETS = 5,
    AI_ABILITY_EVALUATION_MATH_TARGETS = 6,
    AI_ABILITY_EVALUATION_PREPARE = 7,
    AI_ABILITY_EVALUATION_MAP_TARGETS = 8
} ai_ability_evaluation_stage_e;

typedef enum ai_origin_evaluation_stage {
    AI_ORIGIN_EVALUATION_PREPARE = 0,
    AI_ORIGIN_EVALUATION_RANKED = 1,
    AI_ORIGIN_EVALUATION_SCANNED = 2
} ai_origin_evaluation_stage_e;

typedef enum ai_cast_origin_stage {
    AI_CAST_ORIGIN_ACT_BEFORE_MOVE = 0,
    AI_CAST_ORIGIN_MOVE_BEFORE_ACT = 1
} ai_cast_origin_stage_e;

typedef enum ai_charging_stage {
    AI_CHARGING_SONG_DANCE = 0,
    AI_CHARGING_WITH_CHARGE = 1,
    AI_CHARGING_WITHOUT_CHARGE = 2
} ai_charging_stage_e;

s32 ai_ability_outcome_evaluate(void);
s32 ai_move_origins_evaluate(void);
s32 ai_action_evaluate(void);

/* math skill */
enum { AI_MATH_SOURCE_SKILLSET_COUNT = 4, AI_MATH_EXTRA_KNOWN_ABILITY_COUNT = 10 };

extern u8 g_ai_math_source_skillsets[AI_MATH_SOURCE_SKILLSET_COUNT];

/* action ranking */
void ai_selected_action_init(void);
void ai_best_rank_insert(void);
s32 ai_rank_is_higher(const ai_action_data_t* a, const ai_action_data_t* b);
void ai_action_rank_reset(ai_action_data_t* action);

/* simulation */
/* Turn-event iteration limit for one simulated action. */
enum { AI_SIMULATED_TURN_LIMIT = 256 };

typedef enum ai_simulation_phase {
    AI_SIMULATION_PREPARE_ACTION = 0,
    AI_SIMULATION_ADVANCE_TURNS = 1,
    AI_SIMULATION_SCORE_STATE = 2
} ai_simulation_phase_e;

/* Pickup occupant on the current action tile; treasure takes precedence. */
typedef enum ai_tile_pickup_kind {
    AI_TILE_PICKUP_NONE = 0,
    AI_TILE_PICKUP_CRYSTAL = 1,
    AI_TILE_PICKUP_TREASURE = 2
} ai_tile_pickup_kind_e;

/* Selected unit state saved by simulation_unit_state_save and restored by
 * simulation_unit_state_restore. HP occupies unaligned bytes 0x1b..0x1c; max HP/MP are not saved. */
typedef struct ai_extended_snapshot {
    ai_status_snapshot_t status;              /* 0x00 */
    u8 status_ct[STATUS_TIMED_COUNT];         /* 0x08 */
    u8 team_flags;                            /* 0x18 */
    u8 faith;                                 /* 0x19 */
    u8 transparent_removal_flag;              /* 0x1a */
    u8 hp_bytes[2];                           /* 0x1b */
    u8 _padding_1d;                           /* aligns mp */
    u16 mp;                                   /* 0x1e */
    u8 charged_ability_ct;                    /* 0x20 */
    u8 base_attributes[UNIT_ATTRIBUTE_COUNT]; /* 0x21; unit_attribute_index_e */
    u8 attributes[UNIT_ATTRIBUTE_COUNT];      /* 0x24; unit_attribute_index_e */
    u8 ct;                                    /* 0x27 */
    u8 x;                                     /* 0x28 */
    u8 position_bytes[2];                     /* 0x29; packed unit position */
    u8 has_turn;                              /* 0x2b */
    u8 movement_taken;
    u8 action_taken;
    u8 auto_battle_setting; /* 0x2e */
    u8 auto_battle_target;
    u8 initial_team_flags;
    u8 inflicted_status[STATUS_BYTE_COUNT];  /* 0x31 */
    u8 equipment[UNIT_EQUIPMENT_SLOT_COUNT]; /* 0x36 */
    u8 brave;                                /* 0x3d */
    u8 mount_info;
    u8 _padding_3f; /* tail padding to 2-byte alignment */
} ai_extended_snapshot_t;
typedef char ai_extended_snapshot_size_must_be_64[sizeof(ai_extended_snapshot_t) == 64 ? 1 : -1];

/* Sixteen 64-byte snapshots share the 1024-byte scratchpad-save arena used
 * during movement propagation. */
extern ai_extended_snapshot_t g_ai_unit_snapshot_storage[AI_COMPACT_UNIT_COUNT];
extern ai_extended_snapshot_t* g_ai_unit_snapshots;

void ai_simulation_begin(void);
void ai_simulation_end(void);
void ai_acting_unit_place(const ai_coords_t* coordinates);
s32 ai_action_simulate_and_score(void);
void ai_tile_pickup_update(void);

/* facing */
typedef enum ai_direction {
    AI_DIRECTION_SOUTH = 0,
    AI_DIRECTION_WEST = 1,
    AI_DIRECTION_NORTH = 2,
    AI_DIRECTION_EAST = 3,
    AI_DIRECTION_OVERLAP = 4,
    AI_DIRECTION_CHOOSE_AT_WAIT = 5
} ai_direction_e;

s32 ai_wait_facing_choose(void);
ai_direction_e ai_target_facing_find(const ai_coords_t* target, const ai_coords_t* origin);

#endif
