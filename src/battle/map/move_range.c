/* Movement range spreading, pathfinding, teleport chances and path encoding. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/ai_decision.h"
#include "fft/battle_map.h"
#include "fft/command.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/memory.h"
#include "fft/move.h"
#include "fft/projectile.h"
#include "fft/random.h"
#include "fft/skillset.h"
#include "fft/status.h"
#include "fft/target.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_runtime.h"
#include "fft/weather.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Compact movement records have auxiliary panels after the terrain panels. */
enum { MOVE_RECORD_COUNT = 16, MOVE_PANEL_COUNT = BATTLE_MAP_TILE_COUNT + MOVE_RECORD_COUNT };

/* The acting unit's block at the start of the scratchpad (0x1f800000), reached through
 * g_pathfind_work. Only target-proven bytes are named. */
typedef struct pathfind_work {
    u16 target_index;  /* 0x00; target tile index: (level << 8) + y * terrain_width + x */
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
    u8 cost_table;           /* 0x0f; terrain_cost_table_e */
    u8 movement_3;           /* 0x10 */
    u8 cannot_enter_water;   /* 0x11 */
    u8 will_drown;           /* 0x12 */
    u8 will_sink;            /* 0x13 */
    u8 cannot_stay_on_water; /* 0x14 */
    u8 mountable_chocobo;    /* 0x15 */
    u8 can_ride;             /* 0x16 */
    u8 _unused_17;
    u8 terrain_width;   /* 0x18 */
    u8 terrain_depth;   /* 0x19 */
    u8 _unknown_1a;     /* 0x1a; see g_move_config_unknown_1a_value */
    u8 unit_size;       /* 0x1b */
    u8 movement_set_3;  /* 0x1c */
    u8 fly_or_teleport; /* 0x1d */
    u8 source_tile_occupied;
    u8 destination_tile_occupied;
    u8 _unused_20[2];
    u8 stepping_stone;      /* 0x22 */
    u8 ai_propagation_mode; /* 0x23; set only by AI target propagation (path_calculate clears it) */
    u8 movement_set_2;      /* 0x24 */
    u8 can_pass_lava;       /* 0x25 */
} pathfind_work_t;

enum { MOVE_SAVED_SOURCE_HEIGHT = 1, MOVE_SAVED_DESTINATION_HEIGHT = 2 };

/* Shared movement work state reached through g_spread_work.
 *
 * ai_move_propagate_from_target and the jump spreading of path_horizontal_jumps_spread
 * use the same base. Coordinate 16-bit values have both signed and unsigned readers; cast
 * explicitly for signed comparisons. This is a verified prefix, not a claim
 * about the allocation's full size.
 */
typedef struct spread_work {
    battle_map_tile_t* current_tile;     /* 0x00 */
    battle_map_tile_t* destination_tile; /* 0x04 */
    target_panel_t* current_panel;       /* 0x08 */
    target_panel_t* destination_panel;   /* 0x0c */
    target_panel_t* source_panel;        /* 0x10; selected by spread_source_panel_init */
    /* 0x14; base-grid or 512 + record panel, set by path_destination_geometry_init */
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
    s16 work_level;                /* 0x34; read signed by path_destination_geometry_init;
                                    * path_selected_candidate_save copies the low byte */
    /* Signed side differences before water adjustment. Source is exit minus
     * opposite (spread_source_panel_init), reversed in path_source_geometry_init.
     * Destination is opposite minus entry (spread_destination_propagate), reversed in
     * path_destination_geometry_init. */
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
    u8 source_opposite_height;            /* 0x49; the extended samples of path_source_geometry_init
                                           * leave it unchanged */
    u8 destination_entry_height;          /* 0x4a; half-height units */
    u8 destination_opposite_height;       /* 0x4b */
    u8 source_base_height_times_two;      /* 0x4c */
    u8 destination_base_height_times_two; /* 0x4d */
    u8 _unused_4e[2];
    /* 0x50; height scratch: a side height compared with the exit/entry height,
     * a panel max_height_delta, or a unit record index in the jump check. */
    u8 work_height;
    u8 jump_half_height; /* 0x51; intermediate-panel half height (spread_horizontal_jump_check) */
    u8 jump_slope;       /* 0x52; intermediate-panel packed slope */
    u8 jump_unit_size;   /* 0x53; acting-unit body size, copied by spread_horizontal_jump_check */
    /* 0x54; step sources, each in direction index % 4: 0-3 leave from the ground, 4-7 from the top of
     * the unit on the tile, 8 and up from that unit's standing heights (4, 8, 0x1c or 0x24) */
    u8 outer_count;
    /* 0x55; destination candidates tried per source: 2, 4, or 7 with a mount's standing heights */
    u8 inner_count;
    u8 source_half_height;      /* 0x56; tile byte 3, low five bits */
    u8 destination_half_height; /* 0x57 */
    u8 source_slope;            /* 0x58; packed two-bit side multipliers */
    u8 destination_slope;       /* 0x59 */
    u8 budget_matches;          /* 0x5a; candidate budget equals predecessor budget minus step cost */
    u8 candidate_saved;         /* 0x5b; set by path_selected_candidate_save */
    u8 _unused_5c[2];
    u8 path_length;                  /* 0x5e; trace pass count, also each panel mark */
    u8 source_climb;                 /* 0x5f; step bit 2 */
    u8 destination_climb;            /* 0x60; step bit 3 */
    u8 step_distance;                /* 0x61; axis distance, encoded minus one */
    u8 frontier_max_remaining_range; /* 0x62; maximum residual queued for the next pass */
    u8 _unused_63;
    /* 0x64 is |exit - entry| (0 when flying), stored into panel max_height_delta;
     * 0x65-0x67 are the best candidate's tie-breakers (path_selected_candidate_save). */
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
    u8 clearance_height; /* 0x74; ceilings below this plus unit size block the step (path_candidate_update) */
} spread_work_t;
typedef char spread_work_size_must_be_120[(sizeof(spread_work_t) == 120) ? 1 : -1];

/* path_record_t.unit_id_flags. */
enum {
    MOVE_RECORD_UNIT_ID_MASK = 0x1f,
    MOVE_RECORD_FLAG_NOT_MOUNTABLE = 0x20,
    MOVE_RECORD_FLAG_OTHER_TEAM = 0x40, /* team bits differ from the mover's */
    MOVE_RECORD_NONE = 0xff
};

/* Sixteen seven-byte records reached through g_path_records.
 * ai_move_propagate_from_target invalidates byte 3. The ordinary movement producer,
 * spread_unit_to_scratchpad_store, stores heights in half-height units: body
 * height includes a rider adjustment, standing height includes terrain/water/Float adjustments,
 * and top height is their sum. */
typedef struct path_record {
    u8 x;                    /* 0x00 */
    u8 y;                    /* 0x01 */
    u8 higher_elevation : 7; /* 0x02 */
    u8 stepping_stone : 1;
    u8 unit_id_flags;   /* 0x03; MOVE_RECORD_* */
    u8 body_height;     /* 0x04 */
    u8 standing_height; /* 0x05 */
    u8 top_height;      /* 0x06 */
} path_record_t;
typedef char path_record_size_must_be_7[(sizeof(path_record_t) == 7) ? 1 : -1];

/* Half-height samples for paths across slopes and raised units. */
static u8 g_height_offsets[7] = { 0, 6, 1, 2, 3, 4, 5 };

/* The pathfinder divides the 1 KB scratchpad among its config, spread state,
 * compact unit records, terrain costs and per-panel frontier flags. */
static pathfind_work_t* g_pathfind_work = (pathfind_work_t*)getScratchAddr(0);

static spread_work_t* g_spread_work = (spread_work_t*)getScratchAddr(0x60);

static path_record_t* g_path_records = (path_record_t*)getScratchAddr(0x20);

static u8* g_terrain_costs = (u8*)getScratchAddr(0x40);

static u8* g_frontier_flags = (u8*)getScratchAddr(0x80);

/* Terrain costs and pathfinder work cursors. */
extern const u8 g_terrain_movement_cost_tables[][64];
extern u8 g_move_terrain_cost;
extern u8 g_move_weather_terrain_cost;
extern s32 g_move_range_valid;
extern s32 g_move_range_unit_id;
extern u8 g_move_path_scratch;
extern u8* g_move_path_frontier;
extern s32 g_move_path_resume_pass;
extern s32 g_move_path_tile_index;
extern void (*g_move_spread_preset_table[])(void);

/* Provisional: always 0; copied into the move config byte 0x1a, which nothing reads. */
extern u8 g_move_config_unknown_1a_value;

/* Zeroed by path_calculate and never read. That pass keeps in global scratch what
 * spread_unit_to_scratchpad_store keeps in locals; these three stand where it sets
 * its team, ignore_terrain and count locals (the generic unit has no team, terrain rule or units). */
extern u8 g_move_path_team;
extern u8 g_move_path_ignore_terrain;
extern s32 g_move_path_unit_record_count;
static s32 g_ai_move_propagation_resume_pass;

static void path_selected_candidate_apply(spread_work_t* state);

static move_path_t* path_calculate(s32 flags, s32 jump, s32 x, s32 y, u32 level, s32 target_x, s32 target_y,
    u32 target_level, s32 initialize, s32* suspended, s32 check_budget);

static void path_horizontal_jumps_spread(void);

static s32 teleport_roll(void);

static u8 spread_tile_ceiling_calculate(s32 x, s32 y, s32 exit_slope_shift, u8 exit_height);

static void spread_one_tile_around_check(void);

static void spread_occupied_tile_standing_height_check(s32 record_index, s32 extra_span);

static void path_steps_encode(void);

static void path_selected_candidate_save(s32 direction, s32 candidate, s32 extra_span);

static void range_tile_flags_set(void);

static void spread_frontier_flags_set(s32 mode);

static void horizontal_jump_spread(void);

static void path_neighbors_spread(void);

static void spread_tile_height_load(void);

/* Movement cost of the weather-dependent terrains: 1 in clear weather, rain or snow,
 * 2 in a storm or snowstorm, 3 in the strong ones. */
static s32 weather_terrain_cost_get(void) {
    s32 weather = weather_get_effective();

    if (weather < WEATHER_STORM || weather == WEATHER_SNOW) {
        return 1;
    }
    if (weather == WEATHER_STORM || weather == WEATHER_SNOWSTORM) {
        return 2;
    }
    return 3;
}

/* Whether a unit is a chocobo that can be ridden: UNIT_CHOCOBO_MOUNT_NONE for
 * any other job or a chocobo with a status of the unmountable set, PAIRED when
 * its mount_info is set, else FREE. */
unit_chocobo_mount_state_e unit_mount_get_chocobo_state(const unit_stats_t* unit) {
    if (unit->job_id < JOB_ID_CHOCOBO || unit->job_id > JOB_ID_RED_CHOCOBO)
        return UNIT_CHOCOBO_MOUNT_NONE;
    if (status_unit_set_test(unit, STATUS_CHECK_SET_UNMOUNTABLE) != 0)
        return UNIT_CHOCOBO_MOUNT_NONE;
    if (unit->mount_info != 0)
        return UNIT_CHOCOBO_MOUNT_PAIRED;
    return UNIT_CHOCOBO_MOUNT_FREE;
}

/* Fills the pathfinding scratch pad for `unit_id`: its movement, jump and
 * movement-type parameters at 0x1f800000, the per-surface terrain costs, and
 * one seven-byte record per other active unit at 0x1f800080.
 *
 * `value` and `byte` are reused for several unrelated fields; the target keeps
 * each in one register, which decides the allocation of `other` and the
 * standing-height arithmetic.
 *
 * `mount` and `blocked` hold the 0xFF record fill and terrain-cost sentinel,
 * and the cost table base is loaded before its row offset is added: literal
 * constants and `g_terrain_movement_cost_tables[cost_table]` let sched1 reorder the pre-loop
 * `li s2`/`li a3`/`lbu` sequence.
 */
static void spread_unit_to_scratchpad_store(s32 unit_id) {
    pathfind_work_t* config;
    path_record_t* record;
    path_record_t* current;
    unit_stats_t* unit;
    unit_stats_t* other;
    battle_map_tile_t* tile;
    /* Byte access retains the original packed load and field-extraction order. */
    u8 movement[3];
    unit_height_data_t height_data;
    s32 i;
    s32 count;
    s32 weather;
    s32 ignore_terrain;
    const u8* table;
    u8 value;
    u8 movement_3;
    u8 team;
    u8 mount;
    u8 byte;
    s32 base;
    s32 adjust;
    s32 depth;
    s32 blocked;

    config = g_pathfind_work;
    record = g_path_records;
    unit_action_height_calculate(&height_data, unit_id);
    unit = &g_unit_stats[unit_id];
    for (i = 0; i < 3; i++) {
        movement[i] = unit->ability_flags.movement[i];
    }

    config->unit_id = unit_id;
    config->x = unit->x;
    config->y = unit->position.y;
    config->high_elevation = unit->position.higher_elevation;
    config->terrain_width = g_battle_map_terrain_width;
    config->terrain_depth = g_battle_map_terrain_depth;
    config->cannot_enter_water = 0;
    config->cannot_stay_on_water = 0;
    config->will_drown = 1;
    config->will_sink = 1;
    config->fly_or_teleport = 0;
    config->stepping_stone = unit->position.stepping_stone;
    if (movement[2] & (UNIT_MOVEMENT_SET_3_MOVE_ON_LAVA | UNIT_MOVEMENT_SET_3_FLOAT)) {
        config->can_pass_lava = 1;
    } else {
        config->can_pass_lava = 0;
    }

    /* value keeps the status byte with Chicken, Frog and Float for the tests below. */
    value = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)];
    if (value & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
        movement[1] &= ~(UNIT_MOVEMENT_SET_2_TELEPORT_2 | UNIT_MOVEMENT_SET_2_TELEPORT);
        movement[2] &= ~UNIT_MOVEMENT_SET_3_FLY;
        config->unit_size = 4;
    } else {
        config->unit_size = 6;
    }
    if (!(movement[2] & (UNIT_MOVEMENT_SET_3_MOVE_IN_WATER | UNIT_MOVEMENT_SET_3_FLOAT))
        && (movement[1] & UNIT_MOVEMENT_SET_2_CANNOT_ENTER_WATER)) {
        config->cannot_stay_on_water = 1;
    }
    if (movement[2] & UNIT_MOVEMENT_SET_3_FLOAT) {
        config->unit_size += 2;
        config->movement_set_3 = movement[2] | UNIT_MOVEMENT_SET_3_MOVE_IN_WATER;
    } else {
        config->movement_set_3 = movement[2];
    }
    if (value & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
        movement[2] |= UNIT_MOVEMENT_SET_3_FLOAT;
    }
    if (movement[1] & UNIT_MOVEMENT_SET_2_CANNOT_ENTER_WATER) {
        movement[2] &= ~(UNIT_MOVEMENT_SET_3_MOVE_UNDERWATER | UNIT_MOVEMENT_SET_3_WALK_ON_WATER);
    }

    weather = weather_terrain_cost_get();
    if (movement[2] & UNIT_MOVEMENT_SET_3_FLY) {
        config->cost_table = TERRAIN_COST_TABLE_FLY_TELEPORT;
        config->move_type = MOVE_CLASS_FLY;
        config->fly_or_teleport = 1;
    } else if (movement[1] & (UNIT_MOVEMENT_SET_2_TELEPORT | UNIT_MOVEMENT_SET_2_TELEPORT_2)) {
        config->cost_table = TERRAIN_COST_TABLE_FLY_TELEPORT;
        config->movement_set_2 = movement[1];
        config->move_type = MOVE_CLASS_TELEPORT;
        config->fly_or_teleport = 1;
    } else {
        if (movement[2] & UNIT_MOVEMENT_SET_3_FLOAT) {
            config->cost_table = TERRAIN_COST_TABLE_FLOAT;
            config->move_type = MOVE_CLASS_FLOAT;
        } else if (movement[2] & UNIT_MOVEMENT_SET_3_MOVE_ON_LAVA) {
            config->cost_table = TERRAIN_COST_TABLE_LAVA;
            config->move_type = MOVE_CLASS_LAVA;
        } else if (movement[2] & UNIT_MOVEMENT_SET_3_MOVE_IN_WATER) {
            config->cost_table = TERRAIN_COST_TABLE_WATER;
            config->move_type = MOVE_CLASS_WATER_SURFACE;
        } else if (movement[2] & UNIT_MOVEMENT_SET_3_WALK_ON_WATER) {
            config->cost_table = TERRAIN_COST_TABLE_WATER;
            config->move_type = MOVE_CLASS_WATER_DEPTH_ONE;
        } else if (movement[2] & UNIT_MOVEMENT_SET_3_MOVE_UNDERWATER) {
            config->cost_table = TERRAIN_COST_TABLE_UNDERWATER;
            config->move_type = MOVE_CLASS_UNDERWATER;
        } else {
            config->cost_table = TERRAIN_COST_TABLE_DEFAULT;
            config->move_type = MOVE_CLASS_NORMAL;
        }
    }

    if (config->movement_set_3 & (UNIT_MOVEMENT_SET_3_MOVE_IN_WATER | UNIT_MOVEMENT_SET_3_WALK_ON_WATER)) {
        config->will_sink = 0;
    }
    if (config->movement_set_3
        & (UNIT_MOVEMENT_SET_3_MOVE_IN_WATER | UNIT_MOVEMENT_SET_3_WALK_ON_WATER
            | UNIT_MOVEMENT_SET_3_MOVE_UNDERWATER)) {
        config->will_drown = 0;
    }
    if (!(movement[2] & (UNIT_MOVEMENT_SET_3_MOVE_IN_WATER | UNIT_MOVEMENT_SET_3_FLOAT | UNIT_MOVEMENT_SET_3_FLY))
        && (movement[1] & UNIT_MOVEMENT_SET_2_CANNOT_ENTER_WATER)) {
        config->cannot_enter_water = 1;
    }

    config->movement_3 = movement[2];
    if (movement[1] & UNIT_MOVEMENT_SET_2_ANY_WEATHER) {
        weather = 1;
    }

    /* Copy the class's 64 terrain costs, substituting the weather cost and, with
     * Any Ground, 1 for every passable surface. */
    ignore_terrain = movement[1] & UNIT_MOVEMENT_SET_2_ANY_GROUND;
    i = 0;
    blocked = TERRAIN_COST_BLOCKED;
    table = g_terrain_movement_cost_tables[0];
    table += config->cost_table * 64;
    for (; i < 0x40; i++) {
        byte = table[i];
        if (byte == TERRAIN_COST_WEATHER) {
            byte = weather;
        } else if (ignore_terrain && byte != blocked) {
            byte = 1;
        }
        g_terrain_costs[i] = byte;
    }

    /* jump_half is taken before Fly or Ignore Height raises the jump to 0x1F. */
    value = unit->jump;
    if (value >= 8) {
        value = 7;
    }
    config->jump_half = value >> 1;
    if (config->move_type == MOVE_CLASS_FLY || (movement[0] & UNIT_MOVEMENT_SET_1_IGNORE_HEIGHT)) {
        value = 0x1F;
    }
    config->jump_times_two = value * 2;
    config->jump_or_1f = value;

    if (unit_mount_get_chocobo_state(unit) != UNIT_CHOCOBO_MOUNT_NONE) {
        config->mountable_chocobo = 1;
    } else {
        config->mountable_chocobo = 0;
    }

    /* Move is capped at the range of a generic pathfinding search. */
    value = unit->move;
    if (value > MOVE_PATHFIND_RANGE) {
        value = MOVE_PATHFIND_RANGE;
    }
    config->move = value;

    if ((unit->unit_flags & (UNIT_FLAG_MALE | UNIT_FLAG_FEMALE))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)]
            & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG)))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)] & STATUS_BYTE_MASK(STATUS_ID_CHARM))) {
        config->can_ride = 1;
    } else {
        config->can_ride = 0;
    }

    team = unit->initial_team_flags;
    config->_unknown_1a = g_move_config_unknown_1a_value;
    mount = MOVE_RECORD_NONE;
    for (i = MOVE_RECORD_COUNT - 1; i >= 0; i--) {
        record[i].unit_id_flags = mount;
    }

    /* One record per other unit on the map that is not crystal, dead, jumping or treasure. */
    count = 0;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        if (count >= MOVE_RECORD_COUNT) {
            break;
        }
        other = &g_unit_stats[i];
        if (other->active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
            continue;
        }
        if (other->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
            & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD)
                | STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
            continue;
        }
        if (other->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE)) {
            continue;
        }
        if (i == unit_id) {
            continue;
        }

        /* A rider on this unit makes it taller instead of getting a record. */
        mount = other->mount_info;
        if ((mount & UNIT_MOUNT_INFO_FLAG_RIDER) && (mount & UNIT_MOUNT_INFO_PARTNER_ID_MASK) == config->unit_id) {
            config->unit_size += 2;
            continue;
        }

        value = i;
        if (unit_mount_get_chocobo_state(other) != UNIT_CHOCOBO_MOUNT_FREE
            && other->mount_info != (config->unit_id | UNIT_MOUNT_INFO_FLAG_MOUNT)) {
            value = i | MOVE_RECORD_FLAG_NOT_MOUNTABLE;
        }
        byte = other->team_flags;
        if ((team ^ byte) & UNIT_TEAM_MASK) {
            value |= MOVE_RECORD_FLAG_OTHER_TEAM;
        }

        current = &record[count];
        current->unit_id_flags = value;
        current->x = other->x;
        current->y = other->position.y;
        current->higher_elevation = other->position.higher_elevation;
        current->stepping_stone = other->position.stepping_stone;
        value = other->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)];
        if (value & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
            current->body_height = 4;
            current->stepping_stone = 0;
        } else {
            current->body_height = 6;
        }
        value = other->mount_info;
        if ((value & UNIT_MOUNT_INFO_FLAG_MOUNT) && (value & UNIT_MOUNT_INFO_PARTNER_ID_MASK) != config->unit_id) {
            current->body_height += 2;
        }

        /* Standing height in half steps: the tile's surface, raised for Float and for
         * water depth by movement type. */
        movement_3 = other->ability_flags.movement[2];
        tile = &g_battle_map_tile_data[battle_map_tile_calculate_unit_index(other)];
        base = tile->height * 2 + ((*(u8*)&tile->depth_half_height) & BATTLE_MAP_TILE_HALF_HEIGHT_MASK);
        depth = (*(u8*)&tile->depth_half_height) >> 5;
        byte = base;
        if (depth != 0) {
            if (other->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
                adjust = depth * 2 + 2;
                byte = base + adjust;
            } else if (movement_3 & UNIT_MOVEMENT_SET_3_MOVE_IN_WATER) {
                adjust = depth * 2;
                byte = base + adjust;
            } else if (movement_3 & UNIT_MOVEMENT_SET_3_WALK_ON_WATER) {
                adjust = depth * 2 - 2;
                byte = base + adjust;
            }
        } else if (other->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            byte = base + 2;
        }
        current->standing_height = byte;
        current->top_height = byte + current->body_height;
        count++;
    }
}

/*
 * Build the player-side movement range for a unit: reset the pathfinding
 * scratchpad, then spread the movement budget outward one pass per remaining
 * move point, visiting every frontier tile on both levels.  Teleport skips
 * propagation and marks the whole grid reachable.  Returns -1 for an absent
 * unit, otherwise the reachable tile count.
 *
 * The AI twin ai_move_propagate_from_target (0x80178ca4) does the same
 * with a suspendable time budget and a snapshot of the frontier; this one
 * clears frontier_max_remaining_range up front and visits the live frontier
 * directly, so it needs only one set of nested level/y/x loops.
 * The callers also pass map coordinates in a1-a3; the target callee does not
 * read them, but retaining the parameters preserves the original calls.
 */
s32 move_range_build(s32 unit_id, s32 unused_map_x, s32 unused_map_y, s32 unused_map_layer) {
    pathfind_work_t* config = g_pathfind_work;
    spread_work_t* state = g_spread_work;
    u8* frontier = g_frontier_flags;
    s32 pass;

    if (g_unit_stats[unit_id].active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return -1;
    }
    spread_unit_to_scratchpad_store(unit_id);
    spread_frontier_flags_set(MOVE_TILE_FLAGS_MODE_START_SPREAD);
    if (config->move_type == MOVE_CLASS_TELEPORT) {
        u8 remaining_range = 1;
        target_panel_t* fill_panels = g_target_panels;
        for (pass = BATTLE_MAP_TILE_COUNT - 1; pass >= 0; pass--) {
            fill_panels[pass].remaining_range = remaining_range;
        }
    } else {
        for (pass = 0; pass < config->move; pass++) {
            if (!state->frontier_max_remaining_range) {
                break;
            }
            state->frontier_max_remaining_range = 0;
            for (state->tile_level = 0; state->tile_level < BATTLE_MAP_TERRAIN_LAYER_COUNT; state->tile_level++) {
                state->level_offset = state->tile_level * BATTLE_MAP_TILES_PER_LAYER;
                for (state->tile_y = 0; state->tile_y < config->terrain_depth; state->tile_y++) {
                    state->row_offset = state->level_offset + state->tile_y * config->terrain_width;
                    for (state->tile_x = 0; state->tile_x < config->terrain_width; state->tile_x++) {
                        state->tile_index = state->row_offset + state->tile_x;
                        state->current_tile = &g_battle_map_tile_data[state->tile_index];
                        state->current_panel = &g_target_panels[state->tile_index];
                        if (frontier[state->tile_index] & FRONTIER_FLAG_QUEUED) {
                            frontier[state->tile_index] &= ~FRONTIER_FLAG_QUEUED;
                            spread_one_tile_around_check();
                            if (!config->fly_or_teleport) {
                                horizontal_jump_spread();
                            }
                            state->outer_count = 8;
                        }
                    }
                }
            }
        }
    }
    range_tile_flags_set();
    g_move_range_valid = 1;
    g_move_range_unit_id = config->unit_id;
    return state->reachable_count;
}

/* Clears the enterable flag on the tile of every dead or jumping unit. */
static void occupied_enterable_flags_clear(void) {
    s32 unit_id;

    for (unit_id = 0; unit_id < UNIT_SLOT_COUNT; unit_id++) {
        unit_stats_t* unit = &g_unit_stats[unit_id];

        if ((unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE)
            && ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
                    & (STATUS_BYTE_MASK(STATUS_ID_DEAD) | STATUS_BYTE_MASK(STATUS_ID_JUMP)))
                != 0)) {
            g_frontier_flags[battle_map_tile_calculate_unit_index(unit)] &= ~FRONTIER_FLAG_ENTERABLE;
        }
    }
}

/* Rebuild the frontier flags of all terrain tiles from slope, half height, depth
 * and the tile flags, then mark the tiles under the 16 unit records. A unit of
 * another team makes its tile not enterable (a flier may still cross on top of
 * it). One of the mover's team can be mounted unless its record says
 * otherwise, and stood on when it is a stepping stone and the mover is not
 * one. Altima's second form blocks its tile.
 *
 * A mode with bit 0 set also clears the panel ranges and the reachable count.
 * START_SPREAD queues the mover's own tile with Move + 1 range (and the unit
 * record on that tile when it is mountable); TERRAIN_ONLY leaves the units
 * out. */
static void spread_frontier_flags_set(s32 mode) {
    pathfind_work_t* config = g_pathfind_work;
    spread_work_t* state = g_spread_work;
    u8* frontier = g_frontier_flags;
    path_record_t* records = g_path_records;
    path_record_t* record;
    target_panel_t* panel;
    target_panel_t* aux;
    battle_map_tile_t* tile;
    s32 i;
    s32 index;
    s32 height;
    u8* frontier_cell;
    u8* unit_cell;
    u8 slope;
    battle_map_tile_flags_t tile_flags;
    u8 flags;
    u8 depth;
    u8 value;

    for (i = 0, frontier_cell = frontier; i < MOVE_PANEL_COUNT; frontier_cell++, i++) {
        panel = &g_target_panels[i];
        panel->mark = 0;
        panel->unit_record_index = 0;
        if (mode & 1) {
            panel->remaining_range = 0;
            panel->ride_remaining_range = 0;
            panel->max_height_delta = 0xff;
        }
        if (i < BATTLE_MAP_TILE_COUNT) {
            tile = &g_battle_map_tile_data[i];
            frontier[i] = 0;

            /* Bits 0 and 2 are the low bits of the east and west side heights. Exactly one of
             * them (1 or 4) marks a corner shape, low or high; flat tiles and edges give 0 or 5. */
            slope = tile->slope_type & 5;
            if (slope == 1 || slope == 4) {
                *frontier_cell |= FRONTIER_FLAG_CORNER;
                if (tile->depth_half_height.half_height >= 3) {
                    *frontier_cell |= FRONTIER_FLAG_STEEP;
                }
            } else {
                if (tile->depth_half_height.half_height > config->jump_or_1f) {
                    *frontier_cell |= FRONTIER_FLAG_STEEP;
                }
            }
            if ((tile->flags.blocked | tile->flags.untargetable) == 0) {
                depth = tile->depth_half_height.depth;
                if ((depth == 0 || !config->cannot_enter_water) && (depth < 4 || !config->will_sink)) {
                    *frontier_cell |= FRONTIER_FLAG_ENTERABLE;
                }
            }
            tile_flags = tile->flags;
            if (tile_flags.blocked) {
                *frontier_cell |= FRONTIER_FLAG_BLOCKED;
            } else if (!config->move_type && (tile_flags.untargetable)
                && (tile->surface.type) != BATTLE_MAP_SURFACE_CROSS_SECTION) {
                *frontier_cell |= FRONTIER_FLAG_ENTERABLE;
            }
        }
    }
    if (mode != MOVE_TILE_FLAGS_MODE_TERRAIN_ONLY) {
        for (i = 0; i < MOVE_RECORD_COUNT; i++) {
            record = &records[i];
            flags = record->unit_id_flags;
            if (flags != MOVE_RECORD_NONE) {
                index = (record->higher_elevation << 8) + record->y * config->terrain_width + record->x;
                if (g_unit_stats[flags & MOVE_RECORD_UNIT_ID_MASK].character_identity
                    == CHARACTER_IDENTITY_ALTIMA_SECOND_FORM) {
                    frontier[index] = (frontier[index] | FRONTIER_FLAG_BLOCKED) & ~FRONTIER_FLAG_ENTERABLE;
                }
                g_target_panels[index].unit_record_index = i;
                unit_cell = &frontier[index];
                *unit_cell |= FRONTIER_FLAG_UNIT_ON_TILE;
                if (!(flags & MOVE_RECORD_FLAG_OTHER_TEAM)) {
                    if (!config->stepping_stone) {
                        height = record->stepping_stone << 3;
                        *unit_cell = (*unit_cell & ~FRONTIER_FLAG_STAND_ON_UNIT) | height;
                    } else {
                        *unit_cell &= ~FRONTIER_FLAG_STAND_ON_UNIT;
                    }
                    if (!(flags & MOVE_RECORD_FLAG_NOT_MOUNTABLE)) {
                        frontier[index] |= FRONTIER_FLAG_MOUNTABLE_UNIT;
                    } else {
                        frontier[index] &= ~FRONTIER_FLAG_MOUNTABLE_UNIT;
                    }
                } else {
                    *unit_cell &= ~(FRONTIER_FLAG_ENTERABLE | FRONTIER_FLAG_MOUNTABLE_UNIT);
                    if (!config->move_type) {
                        *unit_cell |= FRONTIER_FLAG_STAND_ON_UNIT;
                    } else {
                        *unit_cell &= ~FRONTIER_FLAG_STAND_ON_UNIT;
                    }
                }
            }
        }
    }
    if (mode & 1) {
        state->reachable_count = 0;
        index = (config->high_elevation << 8) + config->y * config->terrain_width + config->x;
    }
    if (mode == MOVE_TILE_FLAGS_MODE_START_SPREAD) {
        panel = &g_target_panels[index];
        if (frontier[index] & FRONTIER_FLAG_MOUNTABLE_UNIT) {
            /* A riding mover shares its tile with its mount's record: also spread from the
             * mount's top and standing heights (spread_one_tile_around_check),
             * seeding the record's panel. */
            state->outer_count = 0x24;
            aux = g_target_panels + BATTLE_MAP_TILE_COUNT + panel->unit_record_index;
            aux->remaining_range = config->move + 1;
            aux->max_height_delta = 0;
        } else {
            state->outer_count = 8;
        }
        state->inner_count = 4;
        panel->remaining_range = config->move + 1;
        panel->max_height_delta = 0;
        frontier[index] |= FRONTIER_FLAG_QUEUED;
        state->frontier_max_remaining_range = config->move + 1;
    }
}

/* Mark every tile the acting unit can end its move on and count them.
 *
 * Teleporting units may land on any valid-destination tile and riders on
 * any mountable panel. Otherwise the tile needs remaining range, a low
 * enough slope, a passable surface and depth the unit tolerates, and must be
 * crossable and either empty or a valid destination the unit can ride onto. */
static void range_tile_flags_set(void) {
    pathfind_work_t* config = g_pathfind_work;
    spread_work_t* spread = g_spread_work;
    u8* flags = g_frontier_flags;
    s32 self = (config->high_elevation << 8) + config->y * config->terrain_width + config->x;
    s32 i;
    battle_map_tile_t* tile;
    target_panel_t* panel;
    s32 half_height;
    s32 surface;
    battle_map_tile_flags_t tile_flags;
    battle_map_tile_depth_t water_depth;
    s32 frontier;

    occupied_enterable_flags_clear();
    for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++) {
        tile = &g_battle_map_tile_data[i];
        tile->ceiling_depth_and_marks.move_destination = 0;
        panel = &g_target_panels[i];
        if (i == self) {
            continue;
        }
        if (!((config->move_type == MOVE_CLASS_TELEPORT) && (flags[i] & FRONTIER_FLAG_MOUNTABLE_UNIT))
            && !((panel->ride_remaining_range != 0) && (config->can_ride != 0))) {
            half_height = tile->depth_half_height.half_height;
            if ((half_height >= 3) && (flags[i] & FRONTIER_FLAG_CORNER)) {
                continue;
            }
            if (panel->remaining_range == 0) {
                continue;
            }
            if (half_height >= 4) {
                continue;
            }
            tile_flags = tile->flags;
            if (tile_flags.cannot_stop) {
                continue;
            }
            surface = tile->surface.type;
            if (surface == BATTLE_MAP_SURFACE_OBSTACLE) {
                continue;
            }
            water_depth = tile->depth_half_height;
            if ((water_depth.depth != 0) && (config->cannot_stay_on_water != 0)) {
                continue;
            }
            if ((water_depth.depth == 3) && (config->will_drown != 0)) {
                continue;
            }
            if ((tile->depth_half_height.depth >= 4) && (config->will_sink != 0)) {
                continue;
            }
            if ((surface == BATTLE_MAP_SURFACE_LAVA) && (config->can_pass_lava == 0)) {
                continue;
            }
            if (tile_flags.untargetable) {
                continue;
            }
            frontier = flags[i];
            if (!(frontier & FRONTIER_FLAG_ENTERABLE)) {
                continue;
            }
            if (frontier & FRONTIER_FLAG_UNIT_ON_TILE) {
                if (!(frontier & FRONTIER_FLAG_MOUNTABLE_UNIT) || (config->can_ride == 0)) {
                    continue;
                }
            }
        }
        spread->reachable_count++;
        tile->ceiling_depth_and_marks.move_destination = 1;
    }
}

/* Step +x: the slope shifts select the source's exit side and the
 * destination's entry side. */
void move_spread_preset_east(void) {
    spread_work_t* spread = g_spread_work;
    spread->x_step = 1;
    spread->y_step = 0;
    spread->source_side_shift = 0;
    spread->destination_side_shift = 2;
}

/* Step +y: the slope shifts select the source's exit side and the
 * destination's entry side. */
void move_spread_preset_north(void) {
    spread_work_t* spread = g_spread_work;
    spread->y_step = 1;
    spread->source_side_shift = 6;
    spread->x_step = 0;
    spread->destination_side_shift = 4;
}

/* Step -x: the slope shifts select the source's exit side and the
 * destination's entry side. */
void move_spread_preset_west(void) {
    spread_work_t* spread = g_spread_work;
    spread->x_step = -1;
    spread->y_step = 0;
    spread->source_side_shift = 2;
    spread->destination_side_shift = 0;
}

/* Step -y: the slope shifts select the source's exit side and the
 * destination's entry side. */
void move_spread_preset_south(void) {
    spread_work_t* spread = g_spread_work;
    spread->y_step = -1;
    spread->source_side_shift = 4;
    spread->x_step = 0;
    spread->destination_side_shift = 6;
}

/* Reset the best-candidate fields of the spread state and load the current
 * tile's heights. */
static void path_spread_scratch_init(void) {
    spread_work_t* spread = g_spread_work;
    spread->best_height_delta = 0xFF;
    spread->best_fly_height_delta = 0xFF;
    spread->budget_matches = 0;
    spread->candidate_saved = 0;
    spread->best_extra_span = 0x7F;
    spread_tile_height_load();
}

/* Load the current tile's base height (doubled, in half-height units), half
 * height and slope into the spread state. */
static void spread_tile_height_load(void) {
    spread_work_t* state = g_spread_work;
    state->source_base_height_times_two = state->current_tile->height << 1;
    state->source_half_height = state->current_tile->depth_half_height.half_height;
    state->source_slope = state->current_tile->slope_type;
}

/* Work tile = current tile + one step of the preset direction. */
static void spread_tile_coordinates_set(void) {
    spread_work_t* spread = g_spread_work;
    spread->work_x = spread->tile_x + spread->x_step;
    spread->work_y = spread->tile_y + spread->y_step;
}

/*
 * Check whether the spreading coordinates lie outside the map.
 *
 * Return 1 for an invalid coordinate, or 0 for an in-bounds tile.
 */
static s32 spread_tile_coordinates_check(void) {
    spread_work_t* state = g_spread_work;
    pathfind_work_t* config = g_pathfind_work;
    s32 x = state->work_x;
    s32 y;
    if (x >= 0) {
        y = state->work_y;
        if (y < 0)
            return 1;
        if (x >= config->terrain_width)
            return 1;
        if (y < config->terrain_depth)
            return 0;
    }
    return 1;
}

/*
 * Prepare the source side of a spread step from the current tile: its exit
 * height, the ceiling over it and the source panel. Directions 0..3 leave from
 * the tile's own exit side, 4..7 from the top of the unit on the tile, and 8 and
 * up from that unit's standing height + direction / 4 - 2 (its record panel).
 *
 * Returns 0 when spreading can continue, 1 when that standing height is below
 * the tile's exit side, 2 when the unit on the tile cannot be stood on, 3 when
 * a steep slope is left toward its high side, and 4 when the source panel has
 * less than 2 range left.
 */
static s32 spread_source_panel_init(s32 direction) {
    pathfind_work_t* config = g_pathfind_work;
    u8* frontier = g_frontier_flags;
    spread_work_t* state = g_spread_work;
    path_record_t* records = g_path_records;

    if (direction >= 8 || state->source_unit_record_flag != 0) {
        s32 panel_index;
        state->source_unit_record_index = state->current_panel->unit_record_index;
        if (direction >= 8) {
            /* Typed pointer addition reverses the target's address operands. */
            path_record_t* record = (path_record_t*)(state->source_unit_record_index * sizeof(*records) + (u32)records);
            state->source_exit_height = record->standing_height + direction / 4 - 2;
            state->work_height = state->source_base_height_times_two
                + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
            if (state->source_exit_height < state->work_height)
                return 1;
        } else {
            if (!(frontier[state->tile_index] & FRONTIER_FLAG_STAND_ON_UNIT))
                return 2;
            state->source_exit_height = records[state->source_unit_record_index].top_height;
        }
        state->source_side_height_delta = 0;
        state->source_opposite_height = state->source_exit_height;
        state->source_ceiling_height = spread_tile_ceiling_calculate(
            state->tile_x, state->tile_y, state->source_side_shift, (state->source_exit_height - 1));
        panel_index = state->source_unit_record_index + BATTLE_MAP_TILE_COUNT;
        state->source_panel_index = panel_index;
        state->source_panel = &g_target_panels[panel_index];
    } else {
        state->source_exit_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
        state->source_opposite_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->destination_side_shift) & 3);
        state->source_side_height_delta = state->source_exit_height - state->source_opposite_height;
        state->source_ceiling_height = spread_tile_ceiling_calculate(
            state->tile_x, state->tile_y, state->source_side_shift, state->source_exit_height);
        if (state->source_side_height_delta >= 0 && (frontier[state->tile_index] & FRONTIER_FLAG_STEEP))
            return 3;
        if (state->current_tile->depth_half_height.depth
            && (config->movement_set_3 & (UNIT_MOVEMENT_SET_3_MOVE_IN_WATER | UNIT_MOVEMENT_SET_3_WALK_ON_WATER))) {
            state->source_exit_height += state->current_tile->depth_half_height.depth * 2;
            if (config->movement_set_3 & UNIT_MOVEMENT_SET_3_WALK_ON_WATER)
                state->source_exit_height -= 2;
        }
        state->source_panel = &g_target_panels[state->source_panel_index = state->tile_index];
    }
    return state->source_panel->remaining_range < 2 ? 4 : 0;
}

/*
 * Check a destination path and propagate an improved movement budget.
 *
 * Return 0 for an accepted path, otherwise a numeric rejection reason.
 * The mount helper can update separate state before a later rejection.
 */
static s32 spread_destination_propagate(s32 candidate, s32 extra_span) {
    spread_work_t* state = g_spread_work;
    pathfind_work_t* config = g_pathfind_work;
    u8* flags = g_frontier_flags;
    path_record_t* records = g_path_records;
    u8* terrain_costs = g_terrain_costs;
    u8* destination_flags;

    state->work_level = candidate & 1;
    state->destination_unit_record_flag = candidate / 2;
    state->destination_tile = &g_battle_map_tile_data[state->destination_index
        = state->work_level * BATTLE_MAP_TILES_PER_LAYER + state->work_y * config->terrain_width + state->work_x];
    state->destination_panel = &g_target_panels[state->destination_index];
    state->destination_base_height_times_two = state->destination_tile->height * 2;
    state->destination_half_height = state->destination_tile->depth_half_height.half_height;
    state->destination_slope = state->destination_tile->slope_type;
    destination_flags = &flags[state->destination_index];
    if (state->destination_unit_record_flag != 0) {
        if (!(*destination_flags & (FRONTIER_FLAG_MOUNTABLE_UNIT | FRONTIER_FLAG_STAND_ON_UNIT)))
            return 1; /* no unit there to mount or stand on */
        state->destination_unit_record_index = state->destination_panel->unit_record_index;
        state->destination_entry_height = records[state->destination_unit_record_index].top_height;
        state->destination_ceiling_height = spread_tile_ceiling_calculate(
            state->work_x, state->work_y, state->destination_side_shift, (state->destination_entry_height - 1));
        if (*destination_flags & FRONTIER_FLAG_MOUNTABLE_UNIT)
            spread_occupied_tile_standing_height_check(state->destination_unit_record_index, extra_span);
        if (!(*destination_flags & FRONTIER_FLAG_STAND_ON_UNIT))
            return 17; /* the unit cannot be stood on (a mount was handled by the helper) */
    } else {
        if (!(*destination_flags & FRONTIER_FLAG_ENTERABLE))
            return 2; /* the tile is not enterable */
        state->destination_entry_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3);
        state->destination_opposite_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->source_side_shift) & 3);
        state->destination_side_height_delta = state->destination_opposite_height - state->destination_entry_height;
        state->destination_ceiling_height = spread_tile_ceiling_calculate(
            state->work_x, state->work_y, state->destination_side_shift, state->destination_entry_height);
        if (state->destination_side_height_delta > 0 && (*destination_flags & FRONTIER_FLAG_STEEP))
            return 3; /* a steep slope entered from its low side */
    }
    if (state->destination_ceiling_height < state->source_exit_height + config->unit_size)
        return 4; /* too low a ceiling over the destination for the mover's exit height */
    if (state->source_ceiling_height < state->destination_entry_height + config->unit_size)
        return 5; /* too low a ceiling over the source for the destination's entry height */
    if (state->destination_ceiling_height < state->destination_entry_height + config->unit_size)
        return 6; /* no headroom on the destination itself */
    if (state->destination_unit_record_flag == 0)
        state->destination_entry_height += state->destination_tile->depth_half_height.depth * 2;
    if (state->destination_entry_height > state->source_exit_height) {
        if (extra_span != 0)
            return 8; /* a step that spans tiles cannot climb */
        state->height_delta = state->destination_entry_height - state->source_exit_height;
    } else {
        state->height_delta = state->source_exit_height - state->destination_entry_height;
    }
    if (state->height_delta > config->jump_times_two)
        return 9; /* beyond the jump limit */
    state->work_height = state->source_panel->max_height_delta;
    state->height_delta = state->work_height < state->height_delta ? state->height_delta : state->work_height;
    if (config->move_type == MOVE_CLASS_FLY)
        state->height_delta = 0;
    if (state->destination_unit_record_flag != 0) {
        /* The record panel's address is recomputed for every access, as retail does. */
        state->candidate_remaining_range = state->source_panel->remaining_range - 1 - extra_span;
        if (state->candidate_remaining_range <= 0)
            return 11; /* no range left to step onto the unit */
        if ((u8)state->candidate_remaining_range
            < g_target_panels[state->destination_unit_record_index + BATTLE_MAP_TILE_COUNT].remaining_range)
            return 12; /* the unit top is already reached with more range left */
        if ((u8)state->candidate_remaining_range
                == g_target_panels[state->destination_unit_record_index + BATTLE_MAP_TILE_COUNT].remaining_range
            && state->height_delta
                >= g_target_panels[state->destination_unit_record_index + BATTLE_MAP_TILE_COUNT].max_height_delta)
            return 13; /* same range left, and no smaller height difference */
        g_target_panels[state->destination_unit_record_index + BATTLE_MAP_TILE_COUNT].remaining_range
            = state->candidate_remaining_range;
        g_target_panels[state->destination_unit_record_index + BATTLE_MAP_TILE_COUNT].max_height_delta
            = state->height_delta;
    } else {
        state->candidate_remaining_range
            = state->source_panel->remaining_range - extra_span - terrain_costs[state->destination_tile->surface.type];
        if (state->candidate_remaining_range <= 0)
            return 14; /* no range left after the terrain cost */
        if ((u8)state->candidate_remaining_range < state->destination_panel->remaining_range)
            return 15; /* the tile is already reached with more range left */
        if ((u8)state->candidate_remaining_range == state->destination_panel->remaining_range
            && state->height_delta >= state->destination_panel->max_height_delta)
            return 16; /* same range left, and no smaller height difference */
        state->destination_panel->remaining_range = state->candidate_remaining_range;
        state->destination_panel->max_height_delta = state->height_delta;
    }
    if (state->candidate_remaining_range >= 2) {
        g_frontier_flags[state->destination_index] |= FRONTIER_FLAG_QUEUED;
        if ((u8)state->candidate_remaining_range > state->frontier_max_remaining_range)
            state->frontier_max_remaining_range = state->candidate_remaining_range;
    }
    return 0;
}

/* Spread one step from the current tile. Source i uses direction i % 4 (east,
 * north, west, south): 0..3 leave from the ground, 4..7 from the top of the
 * unit on the tile, and 8 and up (outer_count 0x24) from that unit's standing
 * height + i / 4 - 2. Each source whose neighbor is on the map tries
 * inner_count destinations: either level, on the ground or on a unit. */
static void spread_one_tile_around_check(void) {
    spread_work_t* state = g_spread_work;
    s32 i;
    s32 j;

    spread_tile_height_load();
    for (i = 0; i < state->outer_count; i++) {
        state->source_unit_record_flag = i >= 4;
        g_move_spread_preset_table[i % 4]();
        spread_tile_coordinates_set();
        if ((spread_tile_coordinates_check() == 0) && (spread_source_panel_init(i) == 0)) {
            for (j = 0; j < state->inner_count; j++) {
                spread_destination_propagate(j, 0);
            }
        }
    }
}

/* Raises the destination panel's ride_remaining_range to the remaining movement budget
 * when a movement record offers a reachable standing height.
 *
 * Candidate heights run from the record's standing height (or the destination
 * surface, if higher) up to its top height. The walk stops at the first height
 * the source or destination ceiling rules out, or a climb that is too high or
 * follows an extra span; a drop beyond the jump budget tries the next height.
 *
 * `height` first carries the record index, as the target keeps both roles in
 * one register. The budget stays an s32 read through u8 casts: a u8 local is
 * born one RTL copy later, and that shorter live range outranks extra_span in
 * global allocation, swapping their registers. */
static void spread_occupied_tile_standing_height_check(s32 record_index, s32 extra_span) {
    spread_work_t* state;
    pathfind_work_t* config;
    path_record_t* records;
    target_panel_t* panel;
    s32 step_cost;
    s32 remaining;
    u8 height = record_index;
    u8 value;
    u8 limit;
    u8 top;

    state = g_spread_work;
    config = g_pathfind_work;
    records = g_path_records;

    if (state->destination_ceiling_height < state->source_exit_height + config->unit_size) {
        return;
    }
    panel = state->source_panel;
    step_cost = extra_span + 1;
    remaining = panel->remaining_range - step_cost;
    if ((u8)remaining == 0) {
        return;
    }
    if ((u8)remaining < panel->ride_remaining_range) {
        return;
    }
    top = state->destination_base_height_times_two
        + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3)
        + state->destination_tile->depth_half_height.depth * 2;
    value = records[height].standing_height;
    limit = records[height].standing_height + records[height].body_height;
    if (value < top) {
        value = top;
    }
    for (height = value; height <= limit; height++) {
        if (state->source_ceiling_height < height + config->unit_size) {
            return;
        }
        if (state->destination_ceiling_height < height + config->unit_size) {
            return;
        }
        if (state->source_exit_height < height) {
            if (extra_span != 0) {
                return;
            }
            if (config->jump_times_two < (u8)(height - state->source_exit_height)) {
                return;
            }
        } else if (config->jump_times_two < (u8)(state->source_exit_height - height)) {
            continue;
        }
        if (state->destination_panel->ride_remaining_range < (u8)remaining) {
            state->destination_panel->ride_remaining_range = remaining;
        }
        return;
    }
}

enum { TILE_CEILING_NONE = 0x7f };

/*
 * Returns the lowest ceiling above (x, y) across the two elevation layers, or
 * 0x7f when no tile above exit_height forms one.  exit_slope_shift selects the
 * exit side's two slope bits; exit_height is the exit side height (or tile
 * height plus unit size).
 */
static u8 spread_tile_ceiling_calculate(s32 x, s32 y, s32 exit_slope_shift, u8 exit_height) {
    s32 elevation;
    battle_map_tile_t* tile;
    battle_map_tile_depth_t depth;
    u8 ceiling = TILE_CEILING_NONE;
    u8 tile_ceiling;

    for (elevation = 0; elevation < BATTLE_MAP_TERRAIN_LAYER_COUNT; elevation++) {
        tile = &g_battle_map_tile_data[elevation * BATTLE_MAP_TILES_PER_LAYER + y * g_pathfind_work->terrain_width + x];
        if ((tile->flags.blocked) == 0) {
            depth = tile->depth_half_height;
            tile_ceiling = (tile->height << 1) + (depth.half_height) * ((tile->slope_type >> exit_slope_shift) & 3)
                - ((tile->ceiling_depth_and_marks.ceiling_depth) << 1);
            if (tile_ceiling > exit_height) {
                if (ceiling < tile_ceiling) {
                    tile_ceiling = ceiling;
                }
                ceiling = tile_ceiling;
            }
        }
    }
    return ceiling;
}

/* Return 1 when the acting unit cannot jump across tile_index at height.
 *
 * Checks both elevation layers, each for its terrain (a blocked frontier tile
 * or the tile's vertical span from its lower side minus ceiling depth to its
 * higher side plus depth) and for a unit of another team standing on it
 * (MOVE_RECORD_FLAG_OTHER_TEAM); the unit check ignores the mover's own team.
 * A span that overlaps the unit's body (height .. height + unit size) blocks
 * the jump; otherwise the jump is blocked only when the lowest span start is
 * above height. shift_a/shift_b select the exit and entry side slope bits.
 *
 * Matching: the a>b arm stages the stored ceiling bound in `high` with the
 * depth term loaded before the store, and the other arm's split `low`
 * assignment keeps `low` ahead of `high` in allocation priority. */
static s32 spread_horizontal_jump_check(s32 tile_index, u8 shift_a, u8 shift_b, u8 height) {
    spread_work_t* state;
    path_record_t* records;
    u8* frontier;
    battle_map_tile_t* tile;
    s32 index;
    s32 layer;
    s32 low;
    u8 high;
    s32 lowest = 0x7f;
    s32 height_a;
    s32 height_b;
    s32 depth;

    frontier = g_frontier_flags;
    state = g_spread_work;
    records = g_path_records;
    state->jump_unit_size = g_pathfind_work->unit_size;
    /* Even passes test a layer's terrain span, odd passes an enemy standing on it. */
    for (layer = 0; layer < 4; layer++) {
        index = (layer / 2) * BATTLE_MAP_TILES_PER_LAYER + tile_index;
        tile = &g_battle_map_tile_data[index];
        if (layer & 1) {
            if ((frontier[index] & (FRONTIER_FLAG_UNIT_ON_TILE | FRONTIER_FLAG_STAND_ON_UNIT))
                != FRONTIER_FLAG_UNIT_ON_TILE) {
                continue;
            }
            state->work_height = g_target_panels[index].unit_record_index;
            if (!(records[state->work_height].unit_id_flags & MOVE_RECORD_FLAG_OTHER_TEAM)) {
                continue;
            }
            low = records[state->work_height].standing_height;
            high = low + 3;
            if (height < high && high < height + state->jump_unit_size) {
                return 1;
            }
            high = records[state->work_height].top_height;
        } else {
            if (frontier[index] & FRONTIER_FLAG_BLOCKED) {
                if (frontier[index] & FRONTIER_FLAG_UNIT_ON_TILE) {
                    return 1;
                }
                continue;
            }
            state->work_height = tile->height * 2;
            state->jump_half_height = tile->depth_half_height.half_height;
            state->jump_slope = tile->slope_type;
            height_a = state->work_height + state->jump_half_height * ((state->jump_slope >> shift_a) & 3);
            height_b = state->work_height + state->jump_half_height * ((state->jump_slope >> shift_b) & 3);
            if ((u8)height_a > (u8)height_b) {
                high = height_b - (tile->ceiling_depth_and_marks.ceiling_depth) * 2;
                depth = (tile->depth_half_height.depth) * 2;
                state->work_height = high;
                low = state->work_height;
                high = height_a + depth;
            } else {
                high = height_b + (tile->depth_half_height.depth) * 2;
                low = height_a;
                low -= (tile->ceiling_depth_and_marks.ceiling_depth) * 2;
            }
        }
        if ((u8)low < (u8)lowest) {
            lowest = low;
        }
        if (height < (u8)low && (u8)low < height + state->jump_unit_size) {
            return 1;
        }
        if (height < high && high < height + state->jump_unit_size) {
            return 1;
        }
        if ((u8)low <= height && height + state->jump_unit_size <= high) {
            return 1;
        }
    }
    if ((u8)lowest != 0x7f) {
        if (height < (u8)lowest) {
            return 1;
        }
    }
    return 0;
}

/* Spread movement across horizontal jumps of up to half the acting unit's Jump.
 *
 * The first four directions spread from the tile and the rest from above the
 * unit (source_unit_record_flag). Each pass continues only while the source panel's remaining
 * range minus the jump span stays at least 2 and both spans are clear. */
static void horizontal_jump_spread(void) {
    pathfind_work_t* config = g_pathfind_work;
    spread_work_t* state = g_spread_work;
    s32 outer_index;
    s32 inner_index;
    s32 pass_index;

    spread_tile_height_load();
    for (outer_index = 0; outer_index < state->outer_count; outer_index++) {
        state->source_unit_record_flag = outer_index >= 4;
        g_move_spread_preset_table[outer_index % 4]();
        if (spread_source_panel_init(outer_index) == 0) {
            spread_tile_coordinates_set();
            for (pass_index = 1; pass_index <= config->jump_half; pass_index++) {
                state->jump_remaining_range = state->source_panel->remaining_range - pass_index;
                if (state->jump_remaining_range < 2) {
                    break;
                }
                if (spread_tile_coordinates_check() != 0) {
                    break;
                }
                if (spread_horizontal_jump_check(state->work_y * config->terrain_width + state->work_x,
                        state->source_side_shift, state->destination_side_shift, state->source_exit_height)
                    != 0) {
                    break;
                }
                state->work_x = state->tile_x + state->x_step * (pass_index + 1);
                state->work_y = state->tile_y + state->y_step * (pass_index + 1);
                if (spread_tile_coordinates_check() != 0) {
                    break;
                }
                for (inner_index = 0; inner_index < state->inner_count; inner_index++) {
                    spread_destination_propagate(inner_index, pass_index);
                }
            }
        }
    }
}

/* Build the acting unit's movement path to a target tile.
 *
 * Returns 0 when the target is not marked reachable, when the unit is not the
 * one the reachable-tile map was computed for, or when an occupied end tile
 * is not a valid destination. Teleport movers get the explicit-destination
 * form (0xfe, x, y, elevation) after the teleport roll, or 0xff on failure.
 * Other movers trace remaining-range values back from the target toward the
 * unit before path_steps_encode builds the step list. Either way the result is
 * the path buffer at g_move_path.
 *
 * g_move_path.flags is written through a pointer loaded twice: the
 * target materializes its address in a register in both blocks. */
move_path_t* move_path_build_to_tile(s32 unit_id, s32 x, s32 y, s32 elevation) {
    move_height_offsets_t no_offsets = { 0 };
    pathfind_work_t* config = g_pathfind_work;
    spread_work_t* state = g_spread_work;
    path_record_t* records = g_path_records;
    u8* frontier = g_frontier_flags;
    s32 index;
    u8 flags;
    u8 source_flags;
    u8 value;
    u8 range;
    u8* move_flags;
    u8 pass;
    s16 previous;
    s16 current;

    config->ai_propagation_mode = 0;
    state->tile_x = config->target_x = x;
    state->tile_y = config->target_y = y;
    state->tile_level = config->target_level = elevation;
    spread_unit_to_scratchpad_store(unit_id);
    index = (config->target_level << 8) + config->target_y * config->terrain_width + config->target_x;
    if (!g_battle_map_tile_data[index].ceiling_depth_and_marks.move_destination) {
        return 0;
    }
    if (g_move_range_valid == 0) {
        return 0;
    }
    if (unit_id != g_move_range_unit_id) {
        return 0;
    }
    if (g_unit_stats[unit_id].active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE) {
        return 0;
    }
    spread_frontier_flags_set(MOVE_TILE_FLAGS_MODE_KEEP_RANGES);
    g_move_path.height_offsets = no_offsets;
    g_move_path.pending_mount = 0;
    config->source_tile_occupied = 0;
    config->destination_tile_occupied = 0;
    state->source_unit_record_flag = 0;
    state->source_unit_record_index = 0;
    state->outer_count = 4;
    index = (config->target_level << 8) + config->target_y * config->terrain_width + config->target_x;
    flags = frontier[index];
    if (flags & FRONTIER_FLAG_UNIT_ON_TILE) {
        if (!(flags & FRONTIER_FLAG_MOUNTABLE_UNIT)) {
            return 0;
        }
        config->destination_tile_occupied = 1;
        state->source_unit_record_index = g_target_panels[index].unit_record_index;
        g_move_path.pending_mount = (records[state->source_unit_record_index].unit_id_flags & MOVE_RECORD_UNIT_ID_MASK)
            + MOVE_PENDING_MOUNT_SET;
        state->outer_count = 0x1c;
    }
    index = (config->high_elevation << 8) + config->y * config->terrain_width + config->x;
    source_flags = frontier[index];
    if (source_flags & FRONTIER_FLAG_UNIT_ON_TILE) {
        if (!(source_flags & FRONTIER_FLAG_MOUNTABLE_UNIT)) {
            return 0;
        }
        config->source_tile_occupied = 1;
    }
    if (config->move_type == MOVE_CLASS_TELEPORT) {
        move_flags = &g_move_path.flags;
        *move_flags = MOVE_EFFECTIVE_TELEPORT;
        value = config->movement_3;
        if (value & UNIT_MOVEMENT_SET_3_MOVE_IN_WATER) {
            *move_flags = MOVE_EFFECTIVE_TELEPORT | MOVE_EFFECTIVE_ON_WATER;
        } else if (value & UNIT_MOVEMENT_SET_3_WALK_ON_WATER) {
            *move_flags = MOVE_EFFECTIVE_TELEPORT | MOVE_EFFECTIVE_IN_WATER;
        }
        if (value & UNIT_MOVEMENT_SET_3_SILENT_WALK) {
            move_flags = &g_move_path.flags;
            *move_flags |= MOVE_EFFECTIVE_SILENT_WALK;
        }
        if (teleport_roll() != 0) {
            g_move_path.count = MOVE_PATH_TELEPORT;
            g_move_path.data.teleport.x = x;
            g_move_path.data.teleport.y = y;
            g_move_path.data.teleport.layer = elevation;
        } else {
            g_move_path.count = MOVE_PATH_TELEPORT_FAILED;
        }
        return &g_move_path;
    }
    state->candidate_remaining_range = 0xff;
    state->path_length = 0;
    state->inner_count = 4;
    state->selected_source_side_shift = 0;

    /* Trace back from the target, numbering each panel with its distance from the end
     * (a mount at the target through its record's ride range, when riding it leaves
     * more range). A tile with no predecessor means no path, unless the mover starts
     * on a mount and a retry against the mount's seven standing heights finds one. */
    while (state->candidate_remaining_range != 0) {
        state->previous_source_side_shift = state->selected_source_side_shift;
        pass = state->path_length + 1;
        state->tile_index
            = state->tile_level * BATTLE_MAP_TILES_PER_LAYER + state->tile_y * config->terrain_width + state->tile_x;
        state->current_tile = &g_battle_map_tile_data[state->tile_index];
        state->current_panel = &g_target_panels[state->tile_index];
        state->path_length = pass;
        if (state->outer_count >= 5) {
            value = state->current_panel->ride_remaining_range;
            range = state->current_panel->remaining_range;
            if (range < value) {
                g_target_panels[state->source_unit_record_index + BATTLE_MAP_TILE_COUNT].ride_remaining_range = pass;
            } else {
                value = range;
                state->source_unit_record_flag = 0;
                state->outer_count = 4;
                state->current_panel->mark = state->path_length;
            }
        } else if (state->source_unit_record_flag != 0) {
            value = g_target_panels[state->source_unit_record_index + BATTLE_MAP_TILE_COUNT].remaining_range;
            g_target_panels[state->source_unit_record_index + BATTLE_MAP_TILE_COUNT].mark = pass;
        } else {
            value = state->current_panel->remaining_range;
            state->current_panel->mark = pass;
        }
        state->candidate_remaining_range = value;
        if (state->tile_x == config->x && state->tile_y == config->y && state->tile_level == config->high_elevation) {
            break;
        }
        path_neighbors_spread();
        previous = state->candidate_remaining_range;
        if (previous != value) {
            state->outer_count = 4;
            continue;
        }
        path_horizontal_jumps_spread();
        current = state->candidate_remaining_range;
        if (current != previous) {
            state->outer_count = 4;
            continue;
        }
        if (config->source_tile_occupied != 0) {
            state->inner_count = 7;
            path_neighbors_spread();
            previous = state->candidate_remaining_range;
            if (previous != current) {
                continue;
            }
            path_horizontal_jumps_spread();
            if (state->candidate_remaining_range != previous) {
                continue;
            }
        }
        return 0;
    }
    path_steps_encode();
    return &g_move_path;
}

/* 1 when the teleport succeeds: always with Teleport 2, otherwise it fails
 * with a 10 in 100 chance per tile of distance beyond Move. */
static s32 teleport_roll(void) {
    pathfind_work_t* ctx = g_pathfind_work;
    s32 dy, dx;

    if (ctx->movement_set_2 & UNIT_MOVEMENT_SET_2_TELEPORT_2) {
        return 1;
    }
    dx = ctx->target_x - ctx->x;
    dy = ctx->target_y - ctx->y;
    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    return random_roll_pass_fail(100, ((dx + dy) - ctx->move) * 10);
}

/* padding */

/* Encode the traced path as move_step_t bytes in g_move_path,
 * count holding the number of encoded steps. Starting from the unit's tile, step n goes to
 * the tile or unit record the back-trace marked with path_length - n; the byte
 * records the direction, the distance, the level, whether the step ends on a
 * unit and whether either end is a climb. */
static void path_steps_encode(void) {
    pathfind_work_t* config = g_pathfind_work;
    spread_work_t* state = g_spread_work;
    path_record_t* records = g_path_records;
    u8* frontier = g_frontier_flags;
    path_record_t* record;
    battle_map_tile_t* tile;
    s32 step;
    s32 path_index;
    u8 level;
    s32 i;
    u8 target;
    s32 rem;
    u8 ride;
    u8 dir;
    s32 flag;
    u8 source_bit;
    u8 destination_bit;

    state->source_unit_record_index = 0;
    for (step = 0; step < state->path_length - 1; step++) {
        state->tile_index
            = state->tile_level * BATTLE_MAP_TILES_PER_LAYER + state->tile_y * config->terrain_width + state->tile_x;
        state->current_tile = &g_battle_map_tile_data[state->tile_index];
        state->current_panel = &g_target_panels[state->tile_index];
        path_index = step + 1;
        target = state->path_length - path_index;
        for (i = 0; i < MOVE_PANEL_COUNT; i++) {
            /* A record panel whose ride range holds the step number is a mount ridden onto. */
            if (i >= BATTLE_MAP_TILE_COUNT && g_target_panels[i].ride_remaining_range == target) {
                ride = 1;
                record = &records[i - BATTLE_MAP_TILE_COUNT];
            } else if (g_target_panels[i].mark == target) {
                ride = 0;
                if (i < BATTLE_MAP_TILE_COUNT) {
                    state->destination_unit_record_index = 0;
                    state->work_level = (u8)(i / BATTLE_MAP_TILES_PER_LAYER);
                    rem = i - (i / BATTLE_MAP_TILES_PER_LAYER) * BATTLE_MAP_TILES_PER_LAYER;
                    state->work_y = (u8)(rem / config->terrain_width);
                    state->work_x = (u8)(rem % config->terrain_width);
                } else {
                    state->destination_unit_record_index = 1;
                    record = &records[i - BATTLE_MAP_TILE_COUNT];
                }
            } else {
                continue;
            }
            if (state->destination_unit_record_index || ride) {
                state->work_x = record->x;
                state->work_y = record->y;
                state->work_level = record->higher_elevation;
            }
            /* dir is the step direction already in bits 6-7 of the step byte. */
            if (state->work_x == state->tile_x) {
                dir = MOVE_STEP_DIRECTION_NORTH << 6;
                if (state->work_y > state->tile_y) {
                    state->source_side_shift = 6;
                    state->destination_side_shift = 4;
                    state->step_distance = state->work_y - state->tile_y;
                } else {
                    dir = MOVE_STEP_DIRECTION_SOUTH << 6;
                    state->source_side_shift = 4;
                    state->destination_side_shift = 6;
                    state->step_distance = state->tile_y - state->work_y;
                }
            } else {
                dir = MOVE_STEP_DIRECTION_EAST << 6;
                if (state->tile_x < state->work_x) {
                    state->source_side_shift = 0;
                    state->destination_side_shift = 2;
                    state->step_distance = state->work_x - state->tile_x;
                } else {
                    dir = MOVE_STEP_DIRECTION_WEST << 6;
                    state->source_side_shift = 2;
                    state->destination_side_shift = 0;
                    state->step_distance = state->tile_x - state->work_x;
                }
            }

            /* A climb end is a slope rising more than the mover's jump, or a corner rising
             * 3 or more; the destination counts only when entered from its low side. */
            state->destination_climb = 0;
            if (!state->destination_unit_record_index && !ride) {
                tile = &g_battle_map_tile_data[i];
                state->destination_entry_height
                    = tile->depth_half_height.half_height * ((tile->slope_type >> state->source_side_shift) & 3);
                state->destination_opposite_height
                    = tile->depth_half_height.half_height * ((tile->slope_type >> state->destination_side_shift) & 3);
                state->destination_side_height_delta
                    = state->destination_entry_height - state->destination_opposite_height;
                if (config->jump_or_1f < tile->depth_half_height.half_height) {
                    state->destination_climb = 1;
                }
                if (tile->depth_half_height.half_height >= 3 && (frontier[i] & FRONTIER_FLAG_CORNER)) {
                    state->destination_climb = 1;
                }
                if (state->destination_side_height_delta >= 0) {
                    state->destination_climb = 0;
                }
            }
            state->source_climb = 0;
            if (!state->source_unit_record_index) {
                if (config->jump_or_1f < state->current_tile->depth_half_height.half_height) {
                    state->source_climb = 1;
                }
                if (state->current_tile->depth_half_height.half_height >= 3
                    && (frontier[state->tile_index] & FRONTIER_FLAG_CORNER)) {
                    state->source_climb = 1;
                }
            }
            level = state->work_level * 32;
            state->source_unit_record_index = state->destination_unit_record_index;
            flag = state->destination_unit_record_index << 4;
            if (step == state->path_length - 2 && g_move_path.height_offsets.source_present) {
                flag = 0x10;
            }
            source_bit = state->source_climb << 2;
            destination_bit = state->destination_climb << 3;
            /* Pack the complete move_step_t: length (distance - 1), slow (source climb), climb
             * (destination climb), mount, higher and direction. Individual field stores add
             * truncation and change scheduling. The final step also sets the mount bit when
             * the back-trace began on a mount at the target (source height offset present). */
            *(u8*)&g_move_path.data.steps[step]
                = (state->step_distance - 1) | (source_bit | (destination_bit | (flag | (dir | level))));
            state->tile_x = state->work_x;
            state->tile_y = state->work_y;
            state->tile_level = state->work_level;
            break;
        }
    }
    g_move_path.count = state->path_length - 1;
    if (config->unit_id >= UNIT_SLOT_COUNT) {
        g_move_path.flags = 0;
    } else {
        g_move_path.flags = move_get_effective_flags(&g_unit_stats[config->unit_id]);
    }
}

/*
 * Prepare source-side heights and ceiling for a movement candidate.
 *
 * Return 1 when the source geometry rejects the candidate, otherwise 0.
 * Extended unit records and ordinary terrain use different height checks;
 * only the terrain path adds water depth after finding the ceiling.
 */
static s32 path_source_geometry_init(s32 direction) {
    spread_work_t* state = g_spread_work;
    u8* frontier = g_frontier_flags;
    path_record_t* records = g_path_records;
    s32 difference;

    if (state->outer_count >= 5) {
        /* Typed pointer addition reverses the target's address operands. */
        path_record_t* record = (path_record_t*)(state->source_unit_record_index * sizeof(*records) + (u32)records);
        state->source_exit_height = record->standing_height + g_height_offsets[6 - direction / 4];
        state->work_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
        if (state->source_exit_height < state->work_height)
            return 1;
        state->source_ceiling_height = spread_tile_ceiling_calculate(state->tile_x, state->tile_y,
            state->source_side_shift, (records[state->source_unit_record_index].standing_height + 5));
        state->source_side_height_delta = 0;
    } else if (state->source_unit_record_flag != 0) {
        if (!(frontier[state->tile_index] & FRONTIER_FLAG_STAND_ON_UNIT))
            return 1;
        state->source_exit_height = records[state->source_unit_record_index].top_height;
        state->source_opposite_height = state->source_exit_height;
        state->source_ceiling_height = spread_tile_ceiling_calculate(
            state->tile_x, state->tile_y, state->source_side_shift, (state->source_exit_height - 1));
        state->source_side_height_delta = 0;
    } else {
        state->source_exit_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->source_side_shift) & 3);
        state->source_opposite_height = state->source_base_height_times_two
            + state->source_half_height * ((state->source_slope >> state->destination_side_shift) & 3);
        state->source_ceiling_height = spread_tile_ceiling_calculate(
            state->tile_x, state->tile_y, state->source_side_shift, state->source_exit_height);
        difference = state->source_opposite_height - state->source_exit_height;
        state->source_side_height_delta = difference;
        if (difference > 0 && (frontier[state->tile_index] & FRONTIER_FLAG_STEEP))
            return 1;
        state->source_exit_height += state->current_tile->depth_half_height.depth * 2;
    }
    return 0;
}

/* Prepare destination geometry for a movement candidate.
 *
 * Candidate bit 0 picks the map layer and candidates 2 and up the top of the
 * unit there; with seven candidates (inner_count 7) each is one of the standing
 * heights of the mount on the mover's own layer. Unit candidates use the
 * record's heights and its record panel. Returns 0 for a usable candidate,
 * otherwise a rejection code (1-5).
 */
static s32 path_destination_geometry_init(s32 candidate) {
    pathfind_work_t* config = g_pathfind_work;
    spread_work_t* state = g_spread_work;
    u8* tile_flags = g_frontier_flags;
    path_record_t* records = g_path_records;

    state->destination_unit_record_flag = candidate >= 2;
    state->work_level = candidate & 1;
    if (state->inner_count >= 5)
        state->work_level = config->high_elevation;
    state->destination_index
        = state->work_level * BATTLE_MAP_TILES_PER_LAYER + state->work_y * config->terrain_width + state->work_x;
    state->destination_tile = &g_battle_map_tile_data[state->destination_index];
    state->destination_panel = &g_target_panels[state->destination_index];
    state->destination_base_height_times_two = state->destination_tile->height * 2;
    state->destination_half_height = state->destination_tile->depth_half_height.half_height;
    state->destination_slope = state->destination_tile->slope_type;
    if (state->inner_count >= 5 || state->destination_unit_record_flag != 0) {
        state->destination_unit_record_index = state->destination_panel->unit_record_index;
        if (state->inner_count >= 5) {
            state->destination_unit_record_flag = 1;
            if (!(tile_flags[state->destination_index] & FRONTIER_FLAG_MOUNTABLE_UNIT))
                return 1;
            state->destination_entry_height
                = records[state->destination_unit_record_index].standing_height + g_height_offsets[candidate];
            state->work_height = state->destination_base_height_times_two
                + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3);
            if (state->destination_entry_height < state->work_height)
                return 2;
        } else {
            if (!(tile_flags[state->destination_index] & FRONTIER_FLAG_STAND_ON_UNIT))
                return 3;
            state->destination_entry_height = records[state->destination_unit_record_index].top_height;
        }
        state->destination_side_height_delta = 0;
        state->destination_opposite_height = state->destination_entry_height;
        state->destination_ceiling_height = spread_tile_ceiling_calculate(
            state->work_x, state->work_y, state->destination_side_shift, (state->destination_entry_height - 1));
        state->destination_panel_index = state->destination_unit_record_index + BATTLE_MAP_TILE_COUNT;
        state->effective_destination_panel = &g_target_panels[state->destination_panel_index];
    } else {
        if (!(tile_flags[state->destination_index] & FRONTIER_FLAG_ENTERABLE))
            return 4;
        state->destination_entry_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->destination_side_shift) & 3);
        state->destination_opposite_height = state->destination_base_height_times_two
            + state->destination_half_height * ((state->destination_slope >> state->source_side_shift) & 3);
        state->destination_side_height_delta = state->destination_entry_height - state->destination_opposite_height;
        state->destination_ceiling_height = spread_tile_ceiling_calculate(
            state->work_x, state->work_y, state->destination_side_shift, state->destination_entry_height);
        if (state->destination_side_height_delta >= 0 && (tile_flags[state->destination_index] & FRONTIER_FLAG_STEEP))
            return 5;
        state->clearance_height = state->source_exit_height;
        if (state->destination_tile->depth_half_height.depth
            && (config->movement_set_3 & (UNIT_MOVEMENT_SET_3_MOVE_IN_WATER | UNIT_MOVEMENT_SET_3_WALK_ON_WATER))) {
            state->destination_entry_height += state->destination_tile->depth_half_height.depth * 2;
            if (config->movement_set_3 & UNIT_MOVEMENT_SET_3_WALK_ON_WATER)
                state->destination_entry_height -= 2;
        }
        state->destination_panel_index = state->destination_index;
        state->effective_destination_panel = &g_target_panels[state->destination_panel_index];
    }
    return 0;
}

/* Check a movement candidate and update its mode-dependent budget state.
 *
 * AI mode returns 11 after an accepted update; panel byte 4 breaks equal
 * budgets by the smaller immediate height difference. Non-AI mode returns 0
 * after geometry checks and sets budget_matches only when the budget matches.
 */
static s32 path_candidate_update(s32 extra_span) {
    spread_work_t* state = g_spread_work;
    pathfind_work_t* config = g_pathfind_work;
    u8* terrain_costs = g_terrain_costs;
    s32 step_cost;

    if (state->source_ceiling_height < state->destination_entry_height + config->unit_size)
        return 1;
    if (state->destination_ceiling_height < state->clearance_height + config->unit_size)
        return 2;
    if (state->source_ceiling_height < state->clearance_height + config->unit_size)
        return 3;
    if (state->destination_entry_height < state->source_exit_height) {
        if (extra_span != 0)
            return 4;
        state->height_delta = state->source_exit_height - state->destination_entry_height;
    } else {
        state->height_delta = state->destination_entry_height - state->source_exit_height;
    }
    if (state->height_delta > config->jump_times_two)
        return 7;

    if (config->ai_propagation_mode != 0) {
        state->candidate_remaining_range
            = state->current_panel->remaining_range - extra_span - terrain_costs[state->current_tile->surface.type];
        if (state->candidate_remaining_range <= 0)
            return 8;
        if ((u8)state->candidate_remaining_range < state->destination_panel->remaining_range)
            return 9;
        if ((u8)state->candidate_remaining_range == state->destination_panel->remaining_range
            && state->height_delta >= state->destination_panel->max_height_delta)
            return 10;
        state->destination_panel->remaining_range = state->candidate_remaining_range;
        state->destination_panel->max_height_delta = state->height_delta;
        if (state->candidate_remaining_range >= 2) {
            g_frontier_flags[state->destination_index] |= FRONTIER_FLAG_QUEUED;
            if ((u8)state->candidate_remaining_range > state->frontier_max_remaining_range)
                state->frontier_max_remaining_range = state->candidate_remaining_range;
        }
        return 11;
    }
    step_cost = extra_span + 1;
    if (state->outer_count >= 5 || state->source_unit_record_flag != 0) {
        if (state->candidate_remaining_range == state->effective_destination_panel->remaining_range - step_cost)
            state->budget_matches = 1;
    } else {
        if (state->candidate_remaining_range
            == state->effective_destination_panel->remaining_range - g_terrain_costs[state->current_tile->surface.type]
                - extra_span)
            state->budget_matches = 1;
    }
    return 0;
}

/* One-tile steps from the current tile to its four neighbors.
 *
 * In AI propagation an accepted step gives the neighbor this tile's range less
 * the step cost when that is more than it has (path_candidate_update).
 * In a back-trace, the best neighbor whose range accounts for the step is saved
 * and becomes the next traced tile.
 */
static void path_neighbors_spread(void) {
    pathfind_work_t* config = g_pathfind_work;
    spread_work_t* state = g_spread_work;
    s32 direction;
    s32 candidate;
    s32 callback;

    path_spread_scratch_init();
    for (direction = 0; direction < state->outer_count; direction++) {
        callback = direction % 4;
        g_move_spread_preset_table[callback]();
        spread_tile_coordinates_set();
        if (spread_tile_coordinates_check() == 0 && path_source_geometry_init(direction) == 0) {
            if (state->inner_count > 4 && (config->x != state->work_x || config->y != state->work_y))
                continue;
            for (candidate = 0; candidate < state->inner_count; candidate++) {
                if (path_destination_geometry_init(candidate) == 0 && path_candidate_update(0) == 0
                    && state->budget_matches != 0)
                    path_selected_candidate_save(direction, candidate, 0);
            }
        }
    }
    if (config->ai_propagation_mode == 0 && state->candidate_saved != 0)
        path_selected_candidate_apply(state);
}

/*
 * Apply the saved movement candidate to the current spreading state.
 *
 * Copy the saved coordinates and residual, then restore half-height offsets
 * when their saved presence bits are set. The destination update uses byte
 * masks because its equivalent bitfield assignments change register allocation.
 */
static void path_selected_candidate_apply(spread_work_t* state) {
    state->candidate_remaining_range = state->saved_remaining_range;
    state->tile_x = state->saved_x;
    state->tile_y = state->saved_y;
    state->tile_level = state->saved_level;
    if (state->saved_height_offset_flags & MOVE_SAVED_SOURCE_HEIGHT) {
        move_height_offsets_t* offsets = &g_move_path.height_offsets;
        move_height_offsets_t packed = *offsets;
        packed.source_height_offset = state->saved_source_height_offset;
        packed.source_present = 1;
        *offsets = packed;
    }
    if (state->saved_height_offset_flags & MOVE_SAVED_DESTINATION_HEIGHT) {
        /* Bit 0 is destination_present, bits 1-3 destination_height_offset. */
        u8* offsets = (u8*)&g_move_path.height_offsets;
        u32 selected = state->saved_destination_height_offset;
        u32 packed = *offsets;
        selected &= 7;
        selected <<= 1;
        packed &= 0xf1;
        *offsets = packed | selected | 1;
    }
    state->source_unit_record_flag = state->saved_unit_record_flag;
    state->source_unit_record_index = state->saved_unit_record_index;
}

/* Horizontal jumps: from the current tile, in each direction, try landing 2 to
 * jump_half + 1 tiles away, provided every tile in between can be jumped over
 * at the landing height (spread_horizontal_jump_check). The best
 * candidate is saved and, outside AI propagation, applied to the spread state. */
static void path_horizontal_jumps_spread(void) {
    pathfind_work_t* config = g_pathfind_work;
    spread_work_t* state = g_spread_work;
    s32 outer_index;
    s32 inner_index;
    s32 pass_index;
    s32 path_index;
    s32 callback_index;

    path_spread_scratch_init();
    for (outer_index = 0; outer_index < state->outer_count; outer_index++) {
        callback_index = outer_index % 4;
        g_move_spread_preset_table[callback_index]();
        if (path_source_geometry_init(outer_index) == 0) {
            pass_index = 1;
            spread_tile_coordinates_set();
            if (config->jump_half != 0) {
                do {
                    if (spread_tile_coordinates_check() == 0) {
                        state->work_x = state->tile_x + state->x_step * (pass_index + 1);
                        state->work_y = state->tile_y + state->y_step * (pass_index + 1);
                        if (spread_tile_coordinates_check() == 0) {
                            for (inner_index = 0; inner_index < state->inner_count; inner_index++) {
                                if (path_destination_geometry_init(inner_index) == 0) {
                                    state->placement_failed = 0;
                                    for (path_index = 1; path_index <= pass_index; path_index++) {
                                        if (spread_horizontal_jump_check(
                                                (state->work_y - state->y_step * path_index) * config->terrain_width
                                                    + (state->work_x - state->x_step * path_index),
                                                state->source_side_shift, state->destination_side_shift,
                                                state->destination_entry_height)
                                            != 0) {
                                            state->placement_failed = 1;
                                            break;
                                        }
                                    }
                                    if (state->placement_failed == 0 && path_candidate_update(pass_index) == 0
                                        && state->budget_matches != 0) {
                                        path_selected_candidate_save(outer_index, inner_index, pass_index);
                                    }
                                }
                            }
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                    pass_index++;
                } while (pass_index <= config->jump_half);
            }
        }
    }
    if (config->ai_propagation_mode == 0 && state->candidate_saved != 0) {
        path_selected_candidate_apply(state);
    }
}

/* Rank and save a movement candidate for the common finalizer.
 *
 * Every attempt clears budget_matches; accepted candidates replace the saved
 * coordinates, selectors and predecessor budget, then set candidate_saved.
 */
static void path_selected_candidate_save(s32 direction, s32 candidate, s32 extra_span) {
    spread_work_t* state = g_spread_work;
    u8 height_delta;

    state->budget_matches = 0;
    if (state->inner_count >= 5) {
        state->saved_remaining_range = state->effective_destination_panel->remaining_range;
    } else {
        state->work_height = state->effective_destination_panel->max_height_delta;
        if (g_pathfind_work->move_type == MOVE_CLASS_FLY) {
            if (state->best_fly_height_delta != 0xff) {
                if (state->selected_source_side_shift == state->previous_source_side_shift
                    && state->source_side_shift != state->previous_source_side_shift)
                    return;
                if ((state->selected_source_side_shift == state->previous_source_side_shift
                        || state->source_side_shift != state->previous_source_side_shift)
                    && state->best_fly_height_delta <= state->height_delta)
                    return;
            }
            height_delta = state->height_delta;
            state->height_delta = 0;
            state->best_fly_height_delta = height_delta;
        } else {
            if (state->best_extra_span < extra_span)
                return;
            if (state->best_extra_span == extra_span) {
                if (state->best_height_delta < state->work_height)
                    return;
                if (state->best_height_delta == state->work_height
                    && state->previous_source_side_shift != state->source_side_shift)
                    return;
            }
            if (state->work_height > state->height_delta)
                state->height_delta = state->work_height;
            state->best_extra_span = extra_span;
        }
        state->best_height_delta = state->height_delta;
        state->selected_source_side_shift = state->source_side_shift;
        state->saved_remaining_range = state->effective_destination_panel->remaining_range;
    }
    state->saved_height_offset_flags = 0;
    state->saved_x = state->work_x;
    state->saved_y = state->work_y;
    state->saved_level = state->work_level;
    /* Extended candidates carry source/destination height selectors. */
    if (state->outer_count >= 5) {
        state->saved_height_offset_flags = MOVE_SAVED_SOURCE_HEIGHT;
        state->saved_source_height_offset = g_height_offsets[6 - direction / 4];
    }
    if (state->inner_count >= 5) {
        state->saved_height_offset_flags |= MOVE_SAVED_DESTINATION_HEIGHT;
        state->saved_destination_height_offset = g_height_offsets[candidate];
    }
    if (state->destination_unit_record_flag != 0) {
        state->saved_unit_record_flag = 1;
        state->saved_unit_record_index = state->destination_unit_record_index;
    } else {
        state->saved_unit_record_flag = 0;
        state->saved_unit_record_index = 0;
    }
    state->candidate_saved = 1;
}

/* path_calculate with the per-call search budget enabled, so a
 * long search can suspend and resume through `suspended`. */
void move_path_calculate_time_sliced(s32 flags, s32 jump, s32 x, s32 y, s32 level, s32 target_x, s32 target_y,
    s32 target_level, s32 initialize, s32* suspended) {
    path_calculate(flags, jump, x, y, level, target_x, target_y, target_level, initialize, suspended, 1);
}

/* Calculate the complete WalkTo path synchronously; return null when no route exists. */
move_path_t* move_path_calculate_walk_to(
    s32 flags, s32 jump, s32 x, s32 y, s32 level, s32 target_x, s32 target_y, s32 target_level) {
    s32 suspended;
    return path_calculate(flags, jump, x, y, level, target_x, target_y, target_level, 1, &suspended, 0);
}

/* Pathfinding pass for a generic unit (id 0x7f) from (x, y, level) to the target
 * tile; returns the route buffer, or 0 when either tile is invalid or unreachable.
 *
 * The 0xff record fill is an s32 local: a literal fill becomes a single-set
 * hoisted pseudo that sched1 places after `li s2,0xf`.
 */
static move_path_t* path_calculate(s32 flags, s32 jump, s32 x, s32 y, u32 level, s32 target_x, s32 target_y,
    u32 target_level, s32 initialize, s32* suspended, s32 check_budget) {
    u8 frontier_snapshot[BATTLE_MAP_TILE_COUNT];
    move_height_offsets_t no_offsets = { 0 };
    pathfind_work_t* config = g_pathfind_work;
    spread_work_t* state = g_spread_work;
    path_record_t* records = g_path_records;
    u8* frontier = g_frontier_flags;
    s32 completed_pass = 0;
    s32 i;
    battle_map_tile_t* tile;
    s32 empty_flags;

    *suspended = 0;
    if (initialize) {
        if (x < 0 || x >= g_battle_map_terrain_width || y < 0 || y >= g_battle_map_terrain_depth
            || level >= BATTLE_MAP_TERRAIN_LAYER_COUNT || target_x < 0 || target_x >= g_battle_map_terrain_width
            || target_y < 0 || target_y >= g_battle_map_terrain_depth
            || target_level >= BATTLE_MAP_TERRAIN_LAYER_COUNT) {
            return 0;
        }
        config->ai_propagation_mode = 0;
        config->x = x;
        config->y = y;
        config->high_elevation = level;
        config->target_x = target_x;
        config->target_y = target_y;
        config->target_level = target_level;
        config->terrain_width = g_battle_map_terrain_width;
        config->terrain_depth = g_battle_map_terrain_depth;
        g_move_path_tile_index = (config->high_elevation << 8) + config->y * config->terrain_width + config->x;
        if (g_battle_map_tile_data[g_move_path_tile_index].flags.blocked
            || g_battle_map_tile_data[g_move_path_tile_index].flags.untargetable) {
            return 0;
        }
        config->target_index
            = (config->target_level << 8) + config->target_y * config->terrain_width + config->target_x;
        if (g_battle_map_tile_data[config->target_index].flags.blocked
            || g_battle_map_tile_data[config->target_index].flags.untargetable) {
            return 0;
        }
        config->unit_id = MOVE_GENERIC_UNIT_ID;
        config->will_drown = 1;
        config->will_sink = 1;
        config->fly_or_teleport = 0;
        config->unit_size = 6;
        config->movement_set_3 = 0;
        g_move_weather_terrain_cost = weather_terrain_cost_get();
        config->cost_table = TERRAIN_COST_TABLE_DEFAULT;
        config->move_type = MOVE_CLASS_NORMAL;
        config->movement_3 = 0;
        g_move_path_ignore_terrain = 0;

        /* g_move_path_scratch holds the cost-table row here, the capped jump
         * next and each traced panel's range in the back-trace. Retail copies 256 costs
         * (four rows) into the 64-byte block, and its Ignore Terrain Cost test compares
         * the row index instead of the cost with 0xff, so blocked surfaces cost 1 too
         * (QUIRKS.md). */
        g_move_path_scratch = config->cost_table;
        for (i = 0; i < 256; i++) {
            g_move_terrain_cost = g_terrain_movement_cost_tables[g_move_path_scratch][i];
            if (g_move_terrain_cost == TERRAIN_COST_WEATHER) {
                g_move_terrain_cost = g_move_weather_terrain_cost;
            }
            if ((flags & MOVE_PATHFIND_IGNORE_TERRAIN_COST) && g_move_path_scratch != 0xFF) {
                g_move_terrain_cost = 1;
            }
            g_terrain_costs[i] = g_move_terrain_cost;
        }
        g_move_path_scratch = jump;
        if (g_move_path_scratch >= 8) {
            g_move_path_scratch = 7;
        }
        config->jump_times_two = g_move_path_scratch * 2;
        config->jump_or_1f = g_move_path_scratch;
        config->jump_half = g_move_path_scratch >> 1;
        config->mountable_chocobo = 0;
        config->move = MOVE_PATHFIND_RANGE;
        config->can_ride = 0;
        g_move_path_team = 0;
        config->_unknown_1a = g_move_config_unknown_1a_value;
        empty_flags = MOVE_RECORD_NONE;
        for (i = MOVE_RECORD_COUNT - 1; i >= 0; i--) {
            records[i].unit_id_flags = empty_flags;
        }
        g_move_path_unit_record_count = 0;
        spread_frontier_flags_set(MOVE_TILE_FLAGS_MODE_TERRAIN_ONLY);
        state->outer_count = 4;
        g_target_panels[g_move_path_tile_index].remaining_range = config->move + 1;
        g_target_panels[g_move_path_tile_index].max_height_delta = 0;
        state->inner_count = 2;
        frontier[g_move_path_tile_index] |= FRONTIER_FLAG_QUEUED;
        state->frontier_max_remaining_range = config->move + 1;
        g_move_path_resume_pass = 0;
    } else {
        mem_copy_byte_data_source_first(g_ai_unit_snapshot_storage, getScratchAddr(0), PSX_SCRATCHPAD_BYTE_COUNT);
    }
    /* Each pass snapshots and clears the queued flags first, so tiles queued while
     * spreading wait for the next pass. */
    for (i = g_move_path_resume_pass; i < config->move; i++) {
        if (check_budget && ai_pathfinding_budget_is_exhausted() && completed_pass) {
            g_move_path_resume_pass = i;
            *suspended = 1;
            mem_copy_byte_data_source_first(getScratchAddr(0), g_ai_unit_snapshot_storage, PSX_SCRATCHPAD_BYTE_COUNT);
            return &g_move_path;
        }
        if (!state->frontier_max_remaining_range) {
            break;
        }
        if (g_target_panels[config->target_index].remaining_range >= state->frontier_max_remaining_range) {
            break;
        }
        for (state->tile_level = 0; state->tile_level < BATTLE_MAP_TERRAIN_LAYER_COUNT; state->tile_level++) {
            state->level_offset = state->tile_level * BATTLE_MAP_TILES_PER_LAYER;
            for (state->tile_y = 0; state->tile_y < config->terrain_depth; state->tile_y++) {
                state->row_offset = state->level_offset + state->tile_y * config->terrain_width;
                for (state->tile_x = 0; state->tile_x < config->terrain_width; state->tile_x++) {
                    state->tile_index = state->row_offset + state->tile_x;
                    state->current_tile = &g_battle_map_tile_data[state->tile_index];
                    state->current_panel = &g_target_panels[state->tile_index];
                    frontier_snapshot[state->tile_index] = frontier[state->tile_index] & FRONTIER_FLAG_QUEUED;
                    frontier[state->tile_index] &= ~FRONTIER_FLAG_QUEUED;
                }
            }
        }
        state->frontier_max_remaining_range = 0;
        for (state->tile_level = 0; state->tile_level < BATTLE_MAP_TERRAIN_LAYER_COUNT; state->tile_level++) {
            state->level_offset = state->tile_level * BATTLE_MAP_TILES_PER_LAYER;
            for (state->tile_y = 0; state->tile_y < config->terrain_depth; state->tile_y++) {
                state->row_offset = state->level_offset + state->tile_y * config->terrain_width;
                for (state->tile_x = 0; state->tile_x < config->terrain_width; state->tile_x++) {
                    state->tile_index = state->row_offset + state->tile_x;
                    state->current_tile = &g_battle_map_tile_data[state->tile_index];
                    state->current_panel = &g_target_panels[state->tile_index];
                    if (frontier_snapshot[state->tile_index]) {
                        spread_one_tile_around_check();
                        if (!config->fly_or_teleport) {
                            horizontal_jump_spread();
                        }
                    }
                }
            }
        }
        completed_pass = 1;
    }

    /* The destination filter of range_tile_flags_set, except that lava
     * is refused only to a flier, which this generic walker never is, so a path may end
     * on lava. */
    for (i = 0; i < BATTLE_MAP_TILE_COUNT; i++) {
        tile = &g_battle_map_tile_data[i];
        tile->ceiling_depth_and_marks.move_destination = 0;
        if (g_target_panels[i].ride_remaining_range != 0
            || ((g_battle_map_tile_data[i].depth_half_height.half_height < 3 || !(frontier[i] & FRONTIER_FLAG_CORNER))
                && g_target_panels[i].remaining_range != 0 && tile->depth_half_height.half_height < 4
                && g_battle_map_tile_data[i].surface.type != BATTLE_MAP_SURFACE_OBSTACLE
                && (tile->depth_half_height.depth == 0 || !config->cannot_stay_on_water)
                && (tile->depth_half_height.depth != 3 || !config->will_drown)
                && (tile->depth_half_height.depth < 4 || !config->will_sink)
                && (tile->surface.type != BATTLE_MAP_SURFACE_LAVA || config->move_type != MOVE_CLASS_FLY)
                && (frontier[i] & FRONTIER_FLAG_ENTERABLE))) {
            state->reachable_count++;
            tile->ceiling_depth_and_marks.move_destination = 1;
        }
    }
    g_move_range_valid = 1;
    g_move_range_unit_id = config->unit_id;
    state->tile_x = config->target_x;
    state->tile_y = config->target_y;
    state->tile_level = config->target_level;
    state->selected_source_side_shift = 0;
    g_move_path_tile_index = (config->target_level << 8) + config->target_y * config->terrain_width + config->target_x;
    if (!(g_battle_map_tile_data[g_move_path_tile_index].ceiling_depth_and_marks.move_destination)) {
        return 0;
    }
    g_move_path_frontier = frontier;
    target_clear_panel_spread_flags();
    g_move_path.height_offsets = no_offsets;
    g_move_path.pending_mount = 0;
    config->source_tile_occupied = 0;
    config->destination_tile_occupied = 0;
    state->outer_count = 4;
    state->candidate_remaining_range = 0xFF;
    state->source_unit_record_flag = 0;
    state->source_unit_record_index = 0;
    state->path_length = 0;
    state->inner_count = 2;

    /* Trace back from the target, numbering each panel with its distance from the end,
     * until the start tile is reached; a tile with no predecessor means no path. */
    while (state->candidate_remaining_range != 0) {
        state->previous_source_side_shift = state->selected_source_side_shift;
        state->tile_index
            = state->tile_level * BATTLE_MAP_TILES_PER_LAYER + state->tile_y * config->terrain_width + state->tile_x;
        state->current_tile = &g_battle_map_tile_data[state->tile_index];
        state->current_panel = &g_target_panels[state->tile_index];
        state->path_length++;
        g_move_path_scratch = state->current_panel->remaining_range;
        state->current_panel->mark = state->path_length;
        state->candidate_remaining_range = g_move_path_scratch;
        if (state->tile_x == config->x && state->tile_y == config->y && state->tile_level == config->high_elevation) {
            break;
        }
        path_neighbors_spread();
        if (state->candidate_remaining_range == g_move_path_scratch) {
            path_horizontal_jumps_spread();
            if (state->candidate_remaining_range == g_move_path_scratch) {
                return 0;
            }
        }
    }
    path_steps_encode();
    return &g_move_path;
}

/* Propagate the acting unit's movement budget outward from a target tile.
 *
 * Each pass snapshots the frontier before visiting it. A time-budget pause
 * saves scratchpad state and sets *suspended to 1 while returning 0.
 * Return -1 for an absent unit on initialization, or the reachable count
 * on completion. The caller must check suspension before using that count.
 */
s32 ai_move_propagate_from_target(
    s32 unit_id, s32 target_x, s32 target_y, s32 target_elevation, s32 initialize, s32* suspended, s32 check_budget) {
    u8 frontier_snapshot[BATTLE_MAP_TILE_COUNT];
    pathfind_work_t* config = g_pathfind_work;
    spread_work_t* state = g_spread_work;
    path_record_t* records = g_path_records;
    u8* frontier = g_frontier_flags;
    s32 completed_pass = 0;
    s32 pass;
    s32 index;
    target_panel_t* panel;

    *suspended = 0;
    if (initialize) {
        if (g_unit_stats[unit_id].active_battle_id == UNIT_ACTIVE_BATTLE_ID_NONE)
            return -1;
        spread_unit_to_scratchpad_store(unit_id);
        config->move = MOVE_PATHFIND_RANGE;
        for (pass = MOVE_RECORD_COUNT - 1; pass >= 0; pass--)
            records[pass].unit_id_flags = MOVE_RECORD_NONE;
        spread_frontier_flags_set(MOVE_TILE_FLAGS_MODE_START_SPREAD);
        index = (config->high_elevation * BATTLE_MAP_TILES_PER_LAYER) + config->y * config->terrain_width + config->x;
        panel = &g_target_panels[index];
        panel->ride_remaining_range = 0;
        panel->remaining_range = 0;
        frontier[index] &= ~FRONTIER_FLAG_QUEUED;
        state->outer_count = 4;
        state->inner_count = 4;
        panel->remaining_range = 0;
        panel->max_height_delta = 0xff;
        index = (target_elevation * BATTLE_MAP_TILES_PER_LAYER) + target_y * config->terrain_width + target_x;
        panel = &g_target_panels[index];
        panel->remaining_range = MOVE_PATHFIND_RANGE + 1;
        frontier[index] |= FRONTIER_FLAG_QUEUED;
        panel->max_height_delta = 0;
        g_ai_move_propagation_resume_pass = 0;
        config->ai_propagation_mode = 1;
    } else {
        /* A direct scratchpad address avoids an extra linker-address addiu. */
        mem_copy_byte_data_source_first(g_ai_unit_snapshot_storage, getScratchAddr(0), PSX_SCRATCHPAD_BYTE_COUNT);
    }
    if (config->move_type == MOVE_CLASS_TELEPORT) {
        /* Teleport and Teleport 2 bypass propagation, not destination filtering. */
        u8 remaining_range = 1;
        target_panel_t* fill_panels = g_target_panels;
        for (pass = BATTLE_MAP_TILE_COUNT - 1; pass >= 0; pass--)
            fill_panels[pass].remaining_range = remaining_range;
    } else {
        for (pass = g_ai_move_propagation_resume_pass; pass < config->move; pass++) {
            if (check_budget && ai_pathfinding_budget_is_exhausted() && completed_pass) {
                g_ai_move_propagation_resume_pass = pass;
                *suspended = 1;
                mem_copy_byte_data_source_first(
                    getScratchAddr(0), g_ai_unit_snapshot_storage, PSX_SCRATCHPAD_BYTE_COUNT);
                return 0;
            }
            if (!state->frontier_max_remaining_range)
                break;
            for (state->tile_level = 0; state->tile_level < BATTLE_MAP_TERRAIN_LAYER_COUNT; state->tile_level++) {
                state->level_offset = state->tile_level * BATTLE_MAP_TILES_PER_LAYER;
                for (state->tile_y = 0; state->tile_y < config->terrain_depth; state->tile_y++) {
                    state->row_offset = state->level_offset + state->tile_y * config->terrain_width;
                    for (state->tile_x = 0; state->tile_x < config->terrain_width; state->tile_x++) {
                        state->tile_index = state->row_offset + state->tile_x;
                        state->current_tile = &g_battle_map_tile_data[state->tile_index];
                        state->current_panel = &g_target_panels[state->tile_index];
                        frontier_snapshot[state->tile_index] = frontier[state->tile_index] & FRONTIER_FLAG_QUEUED;
                        frontier[state->tile_index] &= ~FRONTIER_FLAG_QUEUED;
                    }
                }
            }
            state->frontier_max_remaining_range = 0;
            for (state->tile_level = 0; state->tile_level < BATTLE_MAP_TERRAIN_LAYER_COUNT; state->tile_level++) {
                state->level_offset = state->tile_level * BATTLE_MAP_TILES_PER_LAYER;
                for (state->tile_y = 0; state->tile_y < config->terrain_depth; state->tile_y++) {
                    state->row_offset = state->level_offset + state->tile_y * config->terrain_width;
                    for (state->tile_x = 0; state->tile_x < config->terrain_width; state->tile_x++) {
                        state->tile_index = state->row_offset + state->tile_x;
                        state->current_tile = &g_battle_map_tile_data[state->tile_index];
                        state->current_panel = &g_target_panels[state->tile_index];
                        if (frontier_snapshot[state->tile_index]) {
                            state->source_unit_record_flag = 0;
                            path_neighbors_spread();
                            if (!config->fly_or_teleport) {
                                state->source_unit_record_flag = 0;
                                path_horizontal_jumps_spread();
                            }
                        }
                    }
                }
            }
            completed_pass = 1;
        }
    }
    range_tile_flags_set();
    g_move_range_valid = 1;
    g_move_range_unit_id = config->unit_id;
    return state->reachable_count;
}
