#ifndef BATTLE_MOVE_H
#define BATTLE_MOVE_H

/* Battle movement: movement classes and terrain costs, pathfinding, the steps that carry a unit
 * from tile to tile, teleport, knockback, and what a unit finds where it stops. */

#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "fft/unit.h"
#include "psx/types.h"

/* movement classes and terrain */
/* Ordered class selection in battle_move_store_unit_movement_to_scratchpad, not movement ability
 * flags or the separate cost-table selector. Water names describe standing
 * height. The BATTLE_MOVEMENT_SET_3 bits carry the retail ability names, which
 * are the other way round: "Move in Water" (0x1f6) stands on the surface and
 * "Walk on Water" (0x1f7) one level below it. */
typedef enum move_class {
    MOVE_CLASS_FLY = 0,
    MOVE_CLASS_TELEPORT = 1,
    MOVE_CLASS_FLOAT = 2,
    MOVE_CLASS_LAVA = 3,
    MOVE_CLASS_WATER_SURFACE = 4,
    MOVE_CLASS_WATER_DEPTH_ONE = 5,
    MOVE_CLASS_UNDERWATER = 6,
    MOVE_CLASS_NORMAL = 7,
} move_class_e;

typedef enum move_effective_flags {
    BATTLE_EFFECTIVE_MOVEMENT_SILENT_WALK = 0x02,
    BATTLE_EFFECTIVE_MOVEMENT_TELEPORT = 0x08,
    BATTLE_EFFECTIVE_MOVEMENT_IN_WATER = 0x10, /* BATTLE_MOVEMENT_SET_3_WALK_ON_WATER: one level below the surface */
    BATTLE_EFFECTIVE_MOVEMENT_ON_WATER = 0x20, /* BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER: on the surface */
    BATTLE_EFFECTIVE_MOVEMENT_FLOAT = 0x40,
    BATTLE_EFFECTIVE_MOVEMENT_FLY = 0x80,
} move_effective_flags_e;

extern u8* g_battle_move_terrain_costs_ptr;
extern u8 g_battle_move_terrain_cost;
extern u8 g_battle_move_weather_terrain_cost;

move_effective_flags_e battle_move_get_effective_flags(const unit_stats_t* unit);
s32 battle_move_get_support_flags(const unit_stats_t* unit);
s32 battle_move_get_water_standing_offset(battle_unit_t* unit, map_tile_t* tile);

/* pathfinding */
/* Compact movement records have auxiliary panels after the terrain panels. */
enum {
    MOVE_RECORD_COUNT = 16,
    MOVE_PANEL_COUNT = MAP_TILE_COUNT + MOVE_RECORD_COUNT,
};

/* Generic pathfinding flags, distinct from movement ability bytes. */
typedef enum move_pathfind_flags {
    MOVE_PATHFIND_IGNORE_TERRAIN_COST = 0x100,
} move_pathfind_flags_e;

/* Generic mover and search range used by scripted and reverse pathfinding. */
enum {
    MOVE_GENERIC_UNIT_ID = 0x7f,
    MOVE_PATHFIND_RANGE = 124,
};

/* The acting unit's block at the start of the scratchpad (0x1f800000), reached through
 * g_battle_move_config_ptr. Only target-proven bytes are named. */
typedef struct move_pathfind_scratch {
    u16 target_index;  /* 0x00; target tile index: (level << 8) + y * map_max_x + x */
    u8 jump_times_two; /* 0x02; 0x3e when flying / ignoring height */
    u8 _unused_03;
    u8 jump_or_1f;     /* 0x04 */
    u8 jump_half;      /* 0x05 */
    u8 move;           /* 0x06 */
    u8 x;              /* 0x07; unit_stats_t 0x47 */
    u8 y;              /* 0x08; unit_stats_t 0x48 */
    u8 high_elevation; /* 0x09 */
    u8 target_x;       /* 0x0a */
    u8 target_y;       /* 0x0b */
    u8 target_level;
    u8 unit_id;              /* 0x0d; mount id when riding */
    u8 move_type;            /* 0x0e; move_class_e */
    u8 cost_table;           /* 0x0f; main_terrain_cost_table_e */
    u8 movement_3;           /* 0x10 */
    u8 cannot_enter_water;   /* 0x11 */
    u8 will_drown;           /* 0x12 */
    u8 will_sink;            /* 0x13 */
    u8 cannot_stay_on_water; /* 0x14 */
    u8 mountable_chocobo;    /* 0x15 */
    u8 can_ride;             /* 0x16 */
    u8 _unused_17;
    u8 map_max_x;       /* 0x18 */
    u8 map_max_y;       /* 0x19 */
    u8 _unknown_1a;     /* 0x1a; see g_battle_unknown_8018f4fc */
    u8 unit_size;       /* 0x1b */
    u8 movement_set_3;  /* 0x1c */
    u8 fly_or_teleport; /* 0x1d */
    u8 source_tile_occupied;
    u8 destination_tile_occupied;
    u8 _unused_20[2];
    u8 stepping_stone;      /* 0x22 */
    u8 ai_propagation_mode; /* 0x23; set only by AI target propagation (battle_move_calculate_pathing clears it) */
    u8 movement_set_2;      /* 0x24 */
    u8 can_pass_lava;       /* 0x25 */
} move_pathfind_scratch_t;

enum {
    MOVE_SAVED_SOURCE_HEIGHT = 1,
    MOVE_SAVED_DESTINATION_HEIGHT = 2,
};

/* Per-tile frontier flags produced by the movement spread pass. These are
 * distinct from map_tile_t::ceiling_depth_and_marks and are consumed by the
 * reachable-tile and path-propagation helpers. */
enum {
    FRONTIER_FLAG_QUEUED = 0x01, /* waiting to be spread from in the current pass */
    FRONTIER_FLAG_LOW_CORNER = 0x02,
    FRONTIER_FLAG_UNIT_ON_TILE = 0x04,
    FRONTIER_FLAG_STAND_ON_UNIT = 0x08,  /* the unit on the tile can be stood on */
    FRONTIER_FLAG_ENTERABLE = 0x10,      /* the terrain lets the mover in and no enemy stands there */
    FRONTIER_FLAG_MOUNTABLE_UNIT = 0x20, /* a unit of the mover's team that can be mounted stands there */
    FRONTIER_FLAG_STEEP = 0x40,          /* a slope higher than the mover's jump: no entry from its low side */
    FRONTIER_FLAG_BLOCKED = 0x80,        /* a blocked tile, or the tile of Altima's second form */
};

/* battle_move_set_tile_flags_for_pathfinding modes. Bit 0 clears the target panels' ranges. */
typedef enum move_tile_flags_mode {
    MOVE_TILE_FLAGS_MODE_START_SPREAD = 1, /* mark the units and queue the mover's own tile */
    MOVE_TILE_FLAGS_MODE_KEEP_RANGES = 2,  /* mark the units only */
    MOVE_TILE_FLAGS_MODE_TERRAIN_ONLY = 3, /* leave the units out */
} move_tile_flags_mode_e;

/* Shared movement work state reached through g_battle_move_scratch_pad_ptr.
 *
 * battle_ai_propagate_target_movement and the jump spreading of battle_move_calculate_spread
 * use the same base. Coordinate 16-bit values have both signed and unsigned readers; cast
 * explicitly for signed comparisons. This is a verified prefix, not a claim
 * about the allocation's full size.
 */
typedef struct move_spread_state {
    map_tile_t* current_tile;          /* 0x00 */
    map_tile_t* destination_tile;      /* 0x04 */
    target_panel_t* current_panel;     /* 0x08 */
    target_panel_t* destination_panel; /* 0x0c */
    target_panel_t* source_panel;      /* 0x10; selected by battle_move_init_source_panel */
    /* 0x14; base-grid or 512 + record panel, set by battle_move_init_destination_geometry */
    target_panel_t* effective_destination_panel;
    s16 reachable_count;           /* 0x18 */
    s16 tile_index;                /* 0x1a */
    s16 destination_index;         /* 0x1c */
    s16 source_panel_index;        /* 0x1e; terrain index or 512 + compact record index */
    s16 destination_panel_index;   /* 0x20; terrain index or 512 + compact record index */
    s16 candidate_remaining_range; /* 0x22 */
    s16 jump_remaining_range;      /* 0x24; source remaining range minus the pass (horizontal jump spread) */
    s16 tile_x;                    /* 0x26 */
    s16 tile_y;                    /* 0x28 */
    s16 tile_level;                /* 0x2a */
    u16 level_offset;              /* 0x2c */
    u16 row_offset;                /* 0x2e */
    s16 work_x;                    /* 0x30 */
    s16 work_y;                    /* 0x32 */
    s16 work_level;                /* 0x34; read signed by battle_move_init_destination_geometry;
                                    * battle_move_save_selected_candidate copies the low byte */
    /* Signed side differences before water adjustment. Source is exit minus
     * opposite (battle_move_init_source_panel), reversed in battle_move_init_source_geometry.
     * Destination is opposite minus entry (battle_move_propagate_destination), reversed in
     * battle_move_init_destination_geometry. */
    s16 source_side_height_delta;      /* 0x36 */
    s16 destination_side_height_delta; /* 0x38 */
    s16 x_step;                        /* 0x3a */
    s16 y_step;                        /* 0x3c */
    u8 _unused_3e[2];
    /* 0x40-0x43: whether the source/destination is a unit's compact record
     * (a path above a unit) and that record's index, from panel byte 0x02. */
    u8 source_unit_record_flag;       /* 0x40 */
    u8 source_unit_record_index;      /* 0x41 */
    u8 destination_unit_record_flag;  /* 0x42 */
    u8 destination_unit_record_index; /* 0x43; also the per-step on-record flag in path encoding */
    u8 source_side_shift;
    u8 destination_side_shift;            /* 0x45; slope shift of the destination entry side */
    u8 source_ceiling_height;             /* 0x46; half-height units */
    u8 destination_ceiling_height;        /* 0x47; half-height units */
    u8 source_exit_height;                /* 0x48; half-height units */
    u8 source_opposite_height;            /* 0x49; the extended samples of battle_move_init_source_geometry
                                           * leave it unchanged */
    u8 destination_entry_height;          /* 0x4a; half-height units */
    u8 destination_opposite_height;       /* 0x4b */
    u8 source_base_height_times_two;      /* 0x4c */
    u8 destination_base_height_times_two; /* 0x4d */
    u8 _unused_4e[2];
    /* 0x50; height scratch: a side height compared with the exit/entry height,
     * a panel max_height_delta, or a unit record index in the jump check. */
    u8 work_height;
    u8 jump_half_height;        /* 0x51; intermediate-panel half height (battle_move_check_horizontal_jump) */
    u8 jump_slope;              /* 0x52; intermediate-panel packed slope */
    u8 jump_unit_size;          /* 0x53; acting-unit body size, copied by battle_move_check_horizontal_jump */
    u8 outer_count;             /* 0x54 */
    u8 inner_count;             /* 0x55 */
    u8 source_half_height;      /* 0x56; tile byte 3, low five bits */
    u8 destination_half_height; /* 0x57 */
    u8 source_slope;            /* 0x58; packed two-bit side multipliers */
    u8 destination_slope;       /* 0x59 */
    u8 budget_matches;          /* 0x5a; candidate budget equals predecessor budget minus step cost */
    u8 candidate_saved;         /* 0x5b; set by battle_move_save_selected_candidate */
    u8 _unused_5c[2];
    u8 path_length;                  /* 0x5e; trace pass count, also each panel mark */
    u8 source_climb;                 /* 0x5f; step bit 2 */
    u8 destination_climb;            /* 0x60; step bit 3 */
    u8 step_distance;                /* 0x61; axis distance, encoded minus one */
    u8 frontier_max_remaining_range; /* 0x62; maximum residual queued for the next pass */
    u8 _unused_63;
    /* 0x64 is |exit - entry| (0 when flying), stored into panel max_height_delta;
     * 0x65-0x67 are the best candidate's tie-breakers (battle_move_save_selected_candidate). */
    u8 height_delta;          /* 0x64 */
    u8 best_height_delta;     /* 0x65 */
    u8 best_fly_height_delta; /* 0x66 */
    u8 best_extra_span;       /* 0x67 */
    u8 saved_x;               /* 0x68 */
    u8 saved_y;               /* 0x69 */
    u8 saved_level;           /* 0x6a */
    u8 saved_unit_record_flag;
    u8 saved_unit_record_index;
    u8 saved_remaining_range;
    u8 saved_destination_height_offset;
    u8 saved_source_height_offset;
    u8 saved_height_offset_flags;
    u8 placement_failed; /* 0x71 */
    u8 selected_source_side_shift;
    u8 previous_source_side_shift;
    u8 clearance_height; /* 0x74; ceilings below this plus unit size block the step (battle_move_update_candidate) */
} move_spread_state_t;

typedef char move_spread_state_size_must_be_120[(sizeof(move_spread_state_t) == 120) ? 1 : -1];

/* move_record_t.unit_id_flags. */
enum {
    MOVE_RECORD_UNIT_ID_MASK = 0x1f,
    MOVE_RECORD_FLAG_NOT_MOUNTABLE = 0x20,
    MOVE_RECORD_FLAG_OTHER_TEAM = 0x40, /* team bits differ from the mover's */
    MOVE_RECORD_NONE = 0xff,
};

/* Sixteen seven-byte records reached through g_battle_move_records_ptr.
 * battle_ai_propagate_target_movement invalidates byte 3. The ordinary movement producer,
 * battle_move_store_unit_movement_to_scratchpad, stores heights in half-height units: body
 * height includes a rider adjustment, standing height includes terrain/water/Float adjustments,
 * and top height is their sum. */
typedef struct move_record {
    u8 x;                    /* 0x00 */
    u8 y;                    /* 0x01 */
    u8 higher_elevation : 7; /* 0x02 */
    u8 stepping_stone : 1;
    u8 unit_id_flags;   /* 0x03; MOVE_RECORD_* */
    u8 body_height;     /* 0x04 */
    u8 standing_height; /* 0x05 */
    u8 top_height;      /* 0x06 */
} move_record_t;
typedef char move_record_size_must_be_7[(sizeof(move_record_t) == 7) ? 1 : -1];

extern battle_unit_t* g_battle_move_tile_occupant;
extern s32 g_battle_move_reachable_tiles_valid;
extern s32 g_battle_move_reachable_unit_id;
extern battle_walk_path_t g_battle_move_path;
extern u8 g_battle_move_pathing_scratch;
extern move_pathfind_scratch_t* g_battle_move_config_ptr;
extern u8* g_battle_move_frontier_flags_ptr;

/* Seven proven half-height offsets; the following byte is not
 * established as another element. Extended callers select indices 0..6. */
extern u8 g_battle_move_height_offsets[7];
extern move_record_t* g_battle_move_records_ptr;
extern move_spread_state_t* g_battle_move_scratch_pad_ptr;
extern move_height_offsets_t g_battle_move_path_height_offsets;
extern u8 g_battle_move_destination_unit_record;
extern u8 g_battle_move_effective_flags;
extern u8* g_battle_move_pathing_frontier;
extern s32 g_battle_move_pathing_resume_pass;
extern s32 g_battle_move_pathing_tile_index;

/* Zeroed by battle_move_calculate_pathing and never read. That pass keeps in statics what
 * battle_move_store_unit_movement_to_scratchpad keeps in locals; these three stand where it sets
 * its team, ignore_terrain and count locals (the generic unit has no team, terrain rule or units). */
extern u8 g_battle_move_pathing_team;
extern u8 g_battle_move_pathing_ignore_terrain;
extern s32 g_battle_move_pathing_unit_record_count;
extern void (*g_battle_move_spread_preset_table[])(void);

/* Height scratch record that battle_unit_calculate_height_data fills in place. */
extern battle_unit_height_data_t g_battle_move_tile_occupant_height;

void battle_move_calculate_pathing_time_sliced(s32 flags, s32 jump, s32 x, s32 y, s32 level, s32 target_x, s32 target_y,
    s32 target_level, s32 initialize, s32* suspended);
s32 battle_move_set_reachable_tiles(s32 unit_id, s32 map_x, s32 map_y, s32 map_z);
battle_walk_path_t* battle_move_calculate_walkto_pathing(
    s32 a, s32 b, s32 x, s32 y, s32 z, s32 destination_x, s32 destination_y, s32 destination_z);
battle_walk_path_t* battle_move_build_path_to_tile(s32 unit_id, s32 x, s32 y, s32 elevation);

/* steps and velocity */
extern u8 g_battle_move_current_edge_height;
extern u8 g_battle_move_destination_edge_height;
extern s32 g_battle_move_jump_gravity;
extern move_step_t g_battle_move_step_value;
extern s16 g_battle_move_target_screen_z;

/* move_step_phase_e by step direction: the phase each kind of step enters. */
extern u8 g_battle_move_float_to_center_phases[];
extern u8 g_battle_move_walk_to_center_phases[];
extern u8 g_battle_move_climb_to_center_phases[];
extern u8 g_battle_move_jump_descent_phases[];
extern u8 g_battle_move_jump_landing_phases[];

/* The two map tiles of the step in progress, refreshed by battle_move_get_current_and_destination_tiles
 * from the unit's movement value. Step starters receive the tile pointers; the geometry readers use the
 * tile fields (height at 0x02, depth_half_height at 0x03, slope_type, flags). */
extern map_tile_t* g_battle_move_current_tile;
extern map_tile_t* g_battle_move_destination_tile;

void battle_move_set_velocity_for_contiguous_steps_with_final_tile_height(
    battle_unit_t* unit, const battle_walk_path_t* path, const move_step_t* step);
void battle_move_apply_unit_step_velocity(battle_unit_t* unit);
void battle_move_finish_unit_step_at_tile_edge(battle_unit_t* unit);
void battle_move_update_walking_step_at_tile_edge(battle_unit_t* unit);
void battle_move_animate_move_to_target(battle_unit_t* unit);
void battle_move_animate_move_to_target_level(battle_unit_t* unit);
void battle_move_update_path_step(battle_unit_t* unit);
void battle_move_get_current_and_destination_tiles(
    battle_unit_t* unit, map_tile_t** current_tile, map_tile_t** destination_tile);
s32 battle_move_has_reached_current_tile_exit_edge(s32 direction, battle_unit_t* unit);
s32 battle_move_has_reached_destination_tile_center(s32 direction, battle_unit_t* unit);
s32 battle_move_has_reached_destination_tile_entry_edge(s32 direction, battle_unit_t* unit);

/* Scales each velocity component by scale / ONE. */
void battle_move_interpolate_partial(battle_unit_t* unit, s32 scale);
void battle_move_start_unit_step(battle_unit_t* unit, const map_tile_t* from, const map_tile_t* unused_to);
void battle_move_start_float_step(battle_unit_t* unit, const map_tile_t* from, const map_tile_t* to);
void battle_move_start_unit_step_at_climb_speed(
    battle_unit_t* unit, const map_tile_t* from, const map_tile_t* unused_to);
s32 battle_move_is_unit_moving_by_runtime_id(u32 runtime_id);
void battle_move_snap_axis_to_current_tile_exit_edge(s32 direction, battle_unit_t* unit);
void battle_move_snap_axis_to_destination_tile_center(s32 direction, battle_unit_t* unit);
s32 battle_move_start_unit_jump_by_runtime_id(u32 runtime_id, s32 direction, s32 jump_length);
s32 battle_move_start_unit_walk_to(s32 runtime_id, map_tile_coordinates_t* destination, s32 flags, s32 jump, s32 speed);
void battle_move_animate_move_to_own_tile_level(battle_unit_t* unit);
void battle_move_animate_move_to_own_tile(battle_unit_t* unit);
void battle_move_update_airborne_ascent_phase(battle_unit_t* unit);
void battle_move_update_unit_step_to_destination_tile_center(battle_unit_t* unit);
move_direction_e battle_move_get_direction(const battle_unit_t* unit);
void battle_move_set_velocity_for_contiguous_clamped_steps(
    battle_unit_t* unit, const battle_walk_path_t* path, const move_step_t* step);
void battle_move_start_fly_step(
    battle_unit_t* unit, const map_tile_t* unused_current_tile, const map_tile_t* unused_destination_tile);
void battle_move_update_all_unit_paths(void);
void battle_move_update_float_step_to_current_tile_exit_edge(battle_unit_t* unit);
void battle_move_update_float_step_to_destination_tile_center(battle_unit_t* unit);

/* teleport */
void battle_move_start_teleport_step(battle_unit_t* unit);
void battle_move_start_teleport_animation(battle_unit_t* unit);

/* knockback */
extern s32 g_battle_move_displacement_direction;

void battle_move_displace_unit_at_destination_tile(battle_unit_t* unit, s32 direction);
void battle_move_start_knockback(const battle_unit_t* src, battle_unit_t* dst);
void battle_move_update_knockback_after_animation(battle_unit_t* unit);
void battle_move_update_knockback_step(battle_unit_t* unit);

/* post movement */
/* Eight-byte result copied whole into the post-movement item display. */
typedef struct move_found_item {
    s32 _unknown_00;   /* 0x00; reset when an item is selected */
    u8 item_id;        /* 0x04 */
    u8 _padding_05[3]; /* tail padding to four-byte alignment */
} move_found_item_t;
typedef char move_found_item_size_must_be_8[sizeof(move_found_item_t) == 8 ? 1 : -1];

typedef enum battle_trap_id {
    BATTLE_TRAP_ID_DEGENERATOR = 0,
    BATTLE_TRAP_ID_DEATHTRAP = 1,
    BATTLE_TRAP_ID_SLEEPING_GAS = 2,
    BATTLE_TRAP_ID_STEEL_NEEDLE = 3,
    BATTLE_TRAP_ID_NONE = 255,
} battle_trap_id_e;

typedef enum move_find_result_flags {
    MOVE_FIND_RESULT_ITEM = 0x01,
    MOVE_FIND_RESULT_TRAP = 0x02,
    MOVE_FIND_RESULT_OCCUPIED = 0x04,
} move_find_result_flags_e;

typedef struct move_find_result_data {
    u8 flags; /* move_find_result_flags_e */
    u8 entry_index;
    u8 rare_item_id;
    u8 common_item_id;
    u8 trap_id; /* battle_trap_id_e */
    s8 occupying_unit_id;
} move_find_result_data_t;
typedef char move_find_result_data_size_must_be_6[(sizeof(move_find_result_data_t) == 6) ? 1 : -1];

/* Composite event work produced after movement. This is separate from the
 * tile-query flags in move_find_result_data_t and the learned movement
 * ability bitsets. Bit 0x0200 has readers but no proven producer. */
typedef enum move_post_event_flags {
    MOVE_POST_EVENT_CRYSTAL_OR_TREASURE = 0x0001,
    MOVE_POST_EVENT_MOVEMENT_BENEFIT = 0x0002,
    MOVE_POST_EVENT_ITEM_FOUND = 0x0004,
    MOVE_POST_EVENT_TRAP_TRIGGERED = 0x0008,
    MOVE_POST_EVENT_CHARGING_CANCEL = 0x0010,
    MOVE_POST_EVENT_MOVE_HP_UP = 0x0020,
    MOVE_POST_EVENT_MOVE_MP_UP = 0x0040,
    MOVE_POST_EVENT_MOVE_GET_EXP = 0x0080,
    MOVE_POST_EVENT_MOVE_GET_JP = 0x0100,
    MOVE_POST_EVENT_SOURCE_DISPLAY = 0x0200, /* runs the source unit's post-action display */
    MOVE_POST_EVENT_MOUNT_STATUS_CHANGE = 0x0400,
} move_post_event_flags_e;

/* Contiguous result bytes returned as move_find_result_data_t. Keep
 * distinct symbols: the target reloads their absolute addresses separately. */
extern move_found_item_t g_battle_move_find_display_item;

/* The scalar first-byte alias retains the retail absolute read/write addresses. */
extern move_find_result_data_t g_battle_move_find_tile_result;
extern u8 g_battle_move_find_result_flags;
extern u8 g_battle_move_find_entry_index;
extern u8 g_battle_move_find_rare_item_id;
extern u8 g_battle_move_find_common_item_id;
extern u8 g_battle_move_find_trap_id;
extern s8 g_battle_move_find_occupying_unit_id;

/* Pending move_post_event_flags_e bits, dispatched in priority order. */
extern s32 g_battle_move_find_result;

s32 battle_move_start_next_post_movement_step(void);
void battle_move_init_post_movement_display(void);
u32 battle_move_get_post_move_events(unit_stats_t* unit);

#endif
