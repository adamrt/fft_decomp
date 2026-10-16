#ifndef BATTLE_UNIT_PATH_H
#define BATTLE_UNIT_PATH_H

/* BATTLE unit movement records: destination and motion flags, the walk path and its steps. */

#include "psx/types.h"

/* destination */
/* Masks of the motion flags in the 32-bit view of unit_t.movement (move_destination_t),
 * whose high byte is runtime-unit +0x83. */
typedef enum unit_motion_flags {
    UNIT_MOTION_FLAG_SUPPRESS_FACING_UPDATE = 0x01000000,
    UNIT_MOTION_FLAG_SUPPRESS_SFX_AND_LANDING_EFFECTS = 0x02000000,
    UNIT_MOTION_FLAG_SUPPRESS_PALETTE_UPDATE = 0x04000000,
    UNIT_MOTION_FLAG_ALTERNATE_SHORT_HOP = 0x08000000,
    UNIT_MOTION_FLAG_FLOAT = 0x10000000,
    UNIT_MOTION_FLAG_FORCE_ANIMATION_UPDATE = 0x20000000, /* set with each stored animation request */
} unit_motion_flags_e;

/* Destination bytes and motion flags share one 32-bit movement record. */
typedef struct move_destination {
    u8 destination_x;
    u8 destination_y;
    u8 destination_z;
    u32 suppress_facing_update : 1;
    u32 suppress_sfx_and_landing_effects : 1;
    u32 suppress_palette_update : 1;
    u32 alternate_short_hop : 1;
    u32 floating : 1;
    u32 force_animation_update : 1;
    u32 : 2;
} move_destination_t;
typedef char move_destination_size_must_be_4[sizeof(move_destination_t) == 4 ? 1 : -1];

/* path */
/* move_step_t.direction. */
typedef enum move_step_direction {
    MOVE_STEP_DIRECTION_EAST = 0,  /* +x */
    MOVE_STEP_DIRECTION_WEST = 1,  /* -x */
    MOVE_STEP_DIRECTION_SOUTH = 2, /* -y */
    MOVE_STEP_DIRECTION_NORTH = 3, /* +y */
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

/* Packed half-height offsets (g_move_path_height_offsets). A missing presence flag keeps
 * that side's previous value when move_path_apply_selected_candidate applies a saved candidate. */
typedef struct move_height_offsets {
    u8 destination_present : 1;
    u8 destination_height_offset : 3;
    u8 source_present : 1;
    u8 source_height_offset : 3;
} move_height_offsets_t;
typedef char move_height_offsets_size_must_be_1[(sizeof(move_height_offsets_t) == 1) ? 1 : -1];

/* Tile coordinates stored in teleport paths and relocation reservations. */
typedef struct move_tile_coordinates {
    u8 x;
    u8 y;
    u8 elevation;
} move_tile_coordinates_t;
typedef char battle_tile_coordinates_size_must_be_3[(sizeof(move_tile_coordinates_t) == 3) ? 1 : -1];

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
typedef char battle_walk_path_size_must_be_128[sizeof(move_path_t) == 128 ? 1 : -1];

/* move_path_t.count values that are not a step count
 * (move_path_build_to_tile). */
enum {
    MOVE_PATH_TELEPORT = 0xfe, /* the path bytes hold the destination x, y and level */
    MOVE_PATH_TELEPORT_FAILED = 0xff,
};

/* move_path_t.pending_mount: the mount standing on the path's destination tile. */
enum {
    MOVE_PENDING_MOUNT_ID_MASK = 0x7f,
    MOVE_PENDING_MOUNT_SET = 0x80,
};

/* steps */
/* Travel direction of the step in progress, as move_step_get_direction derives it from the
 * unit's tile and its destination tile. */
typedef enum move_direction {
    MOVE_DIRECTION_SOUTH = 0, /* -y */
    MOVE_DIRECTION_EAST = 1,  /* +x */
    MOVE_DIRECTION_NORTH = 2, /* +y */
    MOVE_DIRECTION_WEST = 3,  /* -x */
} move_direction_e;

/* unit_t.step_phase: the stage of the movement step in progress. Every stage has one
 * value per move_direction_e direction, in that order. The handlers dispatch on the stage and
 * take the direction from the unit's coordinates, and the stores that change stage in the middle of
 * a step write the NORTH value whatever the direction. */
typedef enum move_step_phase {
    MOVE_STEP_PHASE_NONE = 0,
    /* walking: from the tile center to its exit edge, then on to the destination center */
    MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH = 1,
    MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH = 2,
    MOVE_STEP_PHASE_WALK_TO_EDGE_EAST = 3,
    MOVE_STEP_PHASE_WALK_TO_CENTER_EAST = 4,
    MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH = 5,
    MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH = 6,
    MOVE_STEP_PHASE_WALK_TO_EDGE_WEST = 7,
    MOVE_STEP_PHASE_WALK_TO_CENTER_WEST = 8,
    /* the same two stages at climb speed */
    MOVE_STEP_PHASE_CLIMB_TO_EDGE_SOUTH = 9,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH = 10,
    MOVE_STEP_PHASE_CLIMB_TO_EDGE_EAST = 11,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST = 12,
    MOVE_STEP_PHASE_CLIMB_TO_EDGE_NORTH = 13,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH = 14,
    MOVE_STEP_PHASE_CLIMB_TO_EDGE_WEST = 15,
    MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST = 16,
    /* jumping: the take-off animation, rising, falling to the destination edge, the landing animation */
    MOVE_STEP_PHASE_JUMP_START_SOUTH = 17,
    MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH = 18,
    MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH = 19,
    MOVE_STEP_PHASE_JUMP_LANDING_SOUTH = 20,
    MOVE_STEP_PHASE_JUMP_START_EAST = 21,
    MOVE_STEP_PHASE_JUMP_ASCENT_EAST = 22,
    MOVE_STEP_PHASE_JUMP_DESCENT_EAST = 23,
    MOVE_STEP_PHASE_JUMP_LANDING_EAST = 24,
    MOVE_STEP_PHASE_JUMP_START_NORTH = 25,
    MOVE_STEP_PHASE_JUMP_ASCENT_NORTH = 26,
    MOVE_STEP_PHASE_JUMP_DESCENT_NORTH = 27,
    MOVE_STEP_PHASE_JUMP_LANDING_NORTH = 28,
    MOVE_STEP_PHASE_JUMP_START_WEST = 29,
    MOVE_STEP_PHASE_JUMP_ASCENT_WEST = 30,
    MOVE_STEP_PHASE_JUMP_DESCENT_WEST = 31,
    MOVE_STEP_PHASE_JUMP_LANDING_WEST = 32,
    /* flying */
    MOVE_STEP_PHASE_FLY_TO_EDGE_SOUTH = 34,
    MOVE_STEP_PHASE_FLY_TO_CENTER_SOUTH = 35,
    MOVE_STEP_PHASE_FLY_TO_EDGE_EAST = 36,
    MOVE_STEP_PHASE_FLY_TO_CENTER_EAST = 37,
    MOVE_STEP_PHASE_FLY_TO_EDGE_NORTH = 38,
    MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH = 39,
    MOVE_STEP_PHASE_FLY_TO_EDGE_WEST = 40,
    MOVE_STEP_PHASE_FLY_TO_CENTER_WEST = 41,
    /* floating: the two walking stages, and the jump up to a higher tile */
    MOVE_STEP_PHASE_FLOAT_TO_EDGE_SOUTH = 43,
    MOVE_STEP_PHASE_FLOAT_TO_CENTER_SOUTH = 44,
    MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH = 45,
    MOVE_STEP_PHASE_FLOAT_TO_EDGE_EAST = 47,
    MOVE_STEP_PHASE_FLOAT_TO_CENTER_EAST = 48,
    MOVE_STEP_PHASE_FLOAT_JUMP_EAST = 49,
    MOVE_STEP_PHASE_FLOAT_TO_EDGE_NORTH = 51,
    MOVE_STEP_PHASE_FLOAT_TO_CENTER_NORTH = 52,
    MOVE_STEP_PHASE_FLOAT_JUMP_NORTH = 53,
    MOVE_STEP_PHASE_FLOAT_TO_EDGE_WEST = 55,
    MOVE_STEP_PHASE_FLOAT_TO_CENTER_WEST = 56,
    MOVE_STEP_PHASE_FLOAT_JUMP_WEST = 57,
    /* a teleport path (MOVE_PATH_TELEPORT and _FAILED): start the animation, then wait for it */
    MOVE_STEP_PHASE_TELEPORT_START = 59,
    MOVE_STEP_PHASE_TELEPORT_ANIMATION = 60,
} move_step_phase_e;

#endif
