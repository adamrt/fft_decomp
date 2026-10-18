#ifndef MOVE_H
#define MOVE_H

/* Moving a unit: the movement records and paths, movement classes and terrain costs, pathfinding, walks and jumps,
 * knockback, Move-Find items and traps, and the events after a move. */

#include "fft/battle_map.h"
#include "fft/target.h"
#include "fft/unit.h"
#include "psx/types.h"

/* movement records */
/* Destination x/y/layer bytes and motion flags share one 32-bit movement record. */
typedef struct move_destination {
    u8 destination_x;
    u8 destination_y;
    u8 destination_layer;
    u32 suppress_facing_update : 1;
    u32 suppress_sfx_and_landing_effects : 1;
    u32 suppress_palette_update : 1;
    u32 alternate_short_hop : 1;
    u32 floating : 1;
    u32 force_animation_update : 1;
    u32 : 2;
} move_destination_t;
typedef char move_destination_size_must_be_4[sizeof(move_destination_t) == 4 ? 1 : -1];

/* move_step_t.direction. */
typedef enum move_step_direction {
    MOVE_STEP_DIRECTION_EAST = 0,  /* +x */
    MOVE_STEP_DIRECTION_WEST = 1,  /* -x */
    MOVE_STEP_DIRECTION_SOUTH = 2, /* -y */
    MOVE_STEP_DIRECTION_NORTH = 3  /* +y */
} move_step_direction_e;

/* One movement step: a path element, the unit's current step and the step
 * the movement code is working on. */
typedef struct move_step {
    u8 length : 2;    /* extra tiles covered: the jump length */
    u8 slow : 1;      /* start the step at climb speed (move_step_update) */
    u8 climb : 1;     /* continue with a climb-speed step */
    u8 mount : 1;     /* the step ends on another unit */
    u8 higher : 1;    /* destination on the higher elevation */
    u8 direction : 2; /* move_step_direction_e */
} move_step_t;
typedef char move_step_size_must_be_1[sizeof(move_step_t) == 1 ? 1 : -1];

/* Packed half-height offsets (move_path_t.height_offsets). A missing presence flag keeps
 * that side's previous value when move_path_apply_selected_candidate applies a saved candidate. */
typedef struct move_height_offsets {
    u8 destination_present : 1;
    u8 destination_height_offset : 3;
    u8 source_present : 1;
    u8 source_height_offset : 3;
} move_height_offsets_t;
typedef char move_height_offsets_size_must_be_1[(sizeof(move_height_offsets_t) == 1) ? 1 : -1];

/* The complete movement record, copied from g_move_path into unit_t.path.
 * Count 0xfe selects the explicit teleport destination; 0xff is a failed
 * teleport. Ordinary counts select packed steps, each spanning one or more
 * tiles. Byte alignment preserves the original whole-record copies. */
typedef struct move_path {
    u8 count; /* 0x00; step count or MOVE_PATH_TELEPORT/TELEPORT_FAILED */
    union {
        move_step_t steps[0x7c]; /* 0x01; packed direction, elevation and length */
        move_tile_coordinates_t teleport;
    } data;
    u8 flags;                             /* 0x7d; move_effective_flags_e */
    move_height_offsets_t height_offsets; /* 0x7e; source/destination offsets */
    u8 pending_mount;                     /* 0x7f; MOVE_PENDING_MOUNT_SET | battle id */
} move_path_t;
typedef char move_path_size_must_be_128[sizeof(move_path_t) == 128 ? 1 : -1];

/* move_path_t.count values that are not a step count
 * (move_path_build_to_tile). */
enum {
    MOVE_PATH_TELEPORT = 0xfe, /* the path bytes hold the destination x, y and level */
    MOVE_PATH_TELEPORT_FAILED = 0xff
};

/* move_path_t.pending_mount: the mount standing on the path's destination tile. */
enum { MOVE_PENDING_MOUNT_ID_MASK = 0x7f, MOVE_PENDING_MOUNT_SET = 0x80 };

/* movement classes and terrain */
/* Ordered class selection in move_spread_store_unit_to_scratchpad, not movement ability
 * flags or the separate cost-table selector. Water names describe standing
 * height. The UNIT_MOVEMENT_SET_3 bits carry the retail ability names, which
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
    MOVE_CLASS_NORMAL = 7
} move_class_e;

typedef enum move_effective_flags {
    MOVE_EFFECTIVE_SILENT_WALK = 0x02,
    MOVE_EFFECTIVE_TELEPORT = 0x08,
    MOVE_EFFECTIVE_IN_WATER = 0x10, /* UNIT_MOVEMENT_SET_3_WALK_ON_WATER: one level below the surface */
    MOVE_EFFECTIVE_ON_WATER = 0x20, /* UNIT_MOVEMENT_SET_3_MOVE_IN_WATER: on the surface */
    MOVE_EFFECTIVE_FLOAT = 0x40,
    MOVE_EFFECTIVE_FLY = 0x80
} move_effective_flags_e;

move_effective_flags_e move_get_effective_flags(const unit_stats_t* unit);
s32 move_get_ability_events(const unit_stats_t* unit);
s32 move_get_water_standing_offset(unit_t* unit, battle_map_tile_t* tile);

/* pathfinding */
/* Generic pathfinding flags, distinct from movement ability bytes. */
typedef enum move_pathfind_flags { MOVE_PATHFIND_IGNORE_TERRAIN_COST = 0x100 } move_pathfind_flags_e;

/* Generic mover and search range used by scripted and reverse pathfinding. */
enum { MOVE_GENERIC_UNIT_ID = 0x7f, MOVE_PATHFIND_RANGE = 124 };

/* Per-tile frontier flags produced by the movement spread pass. These are
 * distinct from battle_map_tile_t::ceiling_depth_and_marks and are consumed by the
 * reachable-tile and path-propagation helpers. */
enum {
    FRONTIER_FLAG_QUEUED = 0x01, /* waiting to be spread from in the current pass */
    FRONTIER_FLAG_CORNER = 0x02, /* a corner slope, low or high */
    FRONTIER_FLAG_UNIT_ON_TILE = 0x04,
    FRONTIER_FLAG_STAND_ON_UNIT = 0x08,  /* the unit on the tile can be stood on */
    FRONTIER_FLAG_ENTERABLE = 0x10,      /* the terrain lets the mover in and no enemy stands there */
    FRONTIER_FLAG_MOUNTABLE_UNIT = 0x20, /* a unit of the mover's team that can be mounted stands there */
    FRONTIER_FLAG_STEEP = 0x40,          /* a slope higher than the mover's jump: no entry from its low side */
    FRONTIER_FLAG_BLOCKED = 0x80         /* a blocked tile, or the tile of Altima's second form */
};

/* move_spread_set_frontier_flags modes. Bit 0 clears the target panels' ranges. */
typedef enum move_frontier_mode {
    MOVE_TILE_FLAGS_MODE_START_SPREAD = 1, /* mark the units and queue the mover's own tile */
    MOVE_TILE_FLAGS_MODE_KEEP_RANGES = 2,  /* mark the units only */
    MOVE_TILE_FLAGS_MODE_TERRAIN_ONLY = 3  /* leave the units out */
} move_frontier_mode_e;

extern move_path_t g_move_path;

s32 move_range_build(s32 unit_id, s32 unused_map_x, s32 unused_map_y, s32 unused_map_layer);
void move_path_calculate_time_sliced(s32 flags, s32 jump, s32 x, s32 y, s32 level, s32 target_x, s32 target_y,
    s32 target_level, s32 initialize, s32* suspended);
move_path_t* move_path_calculate_walk_to(
    s32 flags, s32 jump, s32 x, s32 y, s32 level, s32 target_x, s32 target_y, s32 target_level);
move_path_t* move_path_build_to_tile(s32 unit_id, s32 x, s32 y, s32 elevation);

/* walks and jumps */
extern s32 g_move_jump_gravity;

s32 move_start_walk_to(
    s32 runtime_id, const battle_map_tile_coordinates_t* destination, s32 flags, s32 jump, s32 speed);
s32 move_start_jump_by_runtime_id(u32 runtime_id, s32 direction, s32 jump_length);
s32 move_is_unit_moving_by_runtime_id(u32 runtime_id);
void move_settle_on_tile(unit_t* unit);

/* knockback */
/* action_current_ability_t.knockback_flags: the kind of knockback found by
 * move_knockback_calculate, with PENDING set until it is applied. */
typedef enum move_knockback_kind {
    MOVE_KNOCKBACK_FALL_DAMAGE = 1,   /* a grounded target: the fall height can hurt */
    MOVE_KNOCKBACK_NO_FALL_DAMAGE = 2 /* a flying target */
} move_knockback_kind_e;

/* Encoding of the kind and pending marker in the same byte. */
enum { MOVE_KNOCKBACK_KIND_MASK = 0x7f, MOVE_KNOCKBACK_PENDING = 0x80 };

void move_knockback_start(const unit_t* src, unit_t* dst);
void move_knockback_update(unit_t* unit);
void move_knockback_cancel(void);

/* Move-Find items and traps */
typedef enum move_find_trap_id {
    MOVE_FIND_TRAP_DEGENERATOR = 0,
    MOVE_FIND_TRAP_DEATHTRAP = 1,
    MOVE_FIND_TRAP_SLEEPING_GAS = 2,
    MOVE_FIND_TRAP_STEEL_NEEDLE = 3,
    MOVE_FIND_TRAP_NONE = 255
} move_find_trap_id_e;

/* Four serialized move-find item/trap entries per map. */
typedef enum move_find_entry_flags {
    BATTLE_MAP_MOVE_FIND_ENTRY_TRAP_DEGENERATOR = 0x01,
    BATTLE_MAP_MOVE_FIND_ENTRY_TRAP_DEATHTRAP = 0x02,
    BATTLE_MAP_MOVE_FIND_ENTRY_TRAP_SLEEPING_GAS = 0x04,
    BATTLE_MAP_MOVE_FIND_ENTRY_TRAP_STEEL_NEEDLE = 0x08,
    BATTLE_MAP_MOVE_FIND_ENTRY_NO_TRAP = 0x10,
    BATTLE_MAP_MOVE_FIND_ENTRY_NO_ITEM = 0x20
} move_find_entry_flags_e;

typedef struct move_find_entry {
    u8 y : 4;
    u8 x : 4;
    u8 flags : 7; /* move_find_entry_flags_e */
    u8 elevation : 1;
    u8 rare_item_id;
    u8 common_item_id;
} move_find_entry_t;
typedef char move_find_entry_size_must_be_4[(sizeof(move_find_entry_t) == 4) ? 1 : -1];

typedef struct move_find_map_data {
    move_find_entry_t entries[4];
} move_find_map_data_t;
typedef char move_find_map_data_size_must_be_16[(sizeof(move_find_map_data_t) == 16) ? 1 : -1];

/* Eight-byte result copied whole into the post-movement item display. */
typedef struct move_found_item {
    s32 _unknown_00;   /* 0x00; reset when an item is selected */
    u8 item_id;        /* 0x04 */
    u8 _padding_05[3]; /* tail padding to four-byte alignment */
} move_found_item_t;
typedef char move_found_item_size_must_be_8[sizeof(move_found_item_t) == 8 ? 1 : -1];

typedef enum move_find_result_flags {
    MOVE_FIND_RESULT_ITEM = 0x01,
    MOVE_FIND_RESULT_TRAP = 0x02,
    MOVE_FIND_RESULT_OCCUPIED = 0x04
} move_find_result_flags_e;

typedef struct move_find_result_data {
    u8 flags; /* move_find_result_flags_e */
    u8 entry_index;
    u8 rare_item_id;
    u8 common_item_id;
    u8 trap_id; /* move_find_trap_id_e */
    s8 occupying_unit_id;
} move_find_result_data_t;
typedef char move_find_result_data_size_must_be_6[(sizeof(move_find_result_data_t) == 6) ? 1 : -1];

/* The current map's entries, copied from g_move_find_map_data when the map loads. */
extern move_find_map_data_t g_move_find_current_map_data;

void move_find_load_item_data(move_find_map_data_t* map_data);
move_find_result_data_t* move_find_get_result(s32 x, u8 y, u8 layer);
move_found_item_t* move_find_determine_item(unit_stats_t* unit);
s32 move_find_prepare_trap_result(unit_stats_t* unit);

/* post movement */
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
    MOVE_POST_EVENT_MOUNT_STATUS_CHANGE = 0x0400
} move_post_event_flags_e;

/* Pending move_post_event_flags_e bits, dispatched in priority order. */
extern s32 g_move_post_events;

u32 move_get_post_events(unit_stats_t* unit);
void move_start_post_events(void);
s32 move_start_next_post_event(void);
s32 move_init_ability_benefit(unit_stats_t* unit);

#endif
