/* Calculate and trace curved and direct projectile trajectories against units and terrain. */

#include "fft/projectile.h"
#include "fft/battle_map.h"
#include "fft/sprite_sheet.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Projectile trajectory queries and work state. */
/* Arc endpoint handed to the per-arc obstacle test: the unit's raised tile position followed by its id and height
 * allowance. */
typedef struct arc_endpoint {
    SVECTOR position;
    s32 unit_id;
    s32 height;
} arc_endpoint_t;

/* Offset to the target plus its fixed-point horizontal length. */
typedef struct arc_delta {
    s32 x;
    s32 y;
    s32 z;
    s32 distance;
} arc_delta_t;

/* Query read by arc_trajectory_calculate: the offset to the target and both endpoints. */
typedef struct arc_query {
    arc_delta_t delta;
    arc_endpoint_t source;
    arc_endpoint_t target;
} arc_query_t;

/* Unit list built in the scratchpad at 0x1f800000 by obstacle_units_list and read by the obstacle tests. */
typedef struct obstacle_unit_list {
    s32 count;                           /* 0x00 */
    u8 battle_ids[UNIT_SLOT_COUNT];      /* 0x04 */
    u8 _padding_19;                      /* 0x19; aligns the position vectors */
    SVECTOR positions[UNIT_SLOT_COUNT];  /* 0x1a; tile center and height */
    u8 _padding_c2[2];                   /* 0xc2; aligns the sprite heights */
    s32 sprite_heights[UNIT_SLOT_COUNT]; /* 0xc4 */
} obstacle_unit_list_t;
typedef char obstacle_unit_list_size_must_be_280[sizeof(obstacle_unit_list_t) == 280 ? 1 : -1];

/* Resident trajectory parameters shared by arc solving, tracing and arrow flight. */
static s32 s_gravity_scale = 0x53a;

static s32 s_gravity_modifier = 0xa8000;

extern s32 g_projectile_launch_slope_q12;
static s16 s_projectile_arctan_angle_mod;
extern s32 g_projectile_source_id;
extern SVECTOR g_projectile_origin;
extern SVECTOR g_projectile_destination;
extern s32 g_projectile_step_count;

/* Last step's movement. */
extern VECTOR g_projectile_step;

/* Final projectile position. */
extern SVECTOR g_projectile_position;

extern s32 g_projectile_hit_unit_id;
extern battle_map_tile_coordinates_t g_projectile_tile;
extern s32 g_projectile_tile_flags;

/* Quadratic height term used by the projectile trajectory calculation;
 * this is not the complete trajectory.
 * distance is the attacker-to-target distance scaled by ONE. */
static s32 arc_height_term_calculate(s32 distance) {
    s32 gravity;

    distance >>= 6;
    gravity = s_gravity_modifier;
    return gravity / 2 - (distance * distance) / (gravity >> 11);
}

/* Solve the two launch slopes of an arcing projectile toward the target.
 *
 * Tries aim offsets of half, three quarters, then zero of the target's
 * sprite height until both the arc apex and the target tile floor clear it,
 * storing the chosen height back into the target endpoint. Returns 0 when the
 * target is at zero distance or no height clears. */
static s32 arc_trajectory_calculate(arc_query_t* query, s32* arc_a, s32* arc_b) {
    battle_map_tile_t* tile;
    s32 tile_height;
    s32 floor;
    s32 peak;
    s32 height;
    s32 limit;
    s32 root;
    s32 gravity;
    s32 distance;
    s32 result = 0;

    tile = battle_map_unit_tile_data_get(query->target.unit_id);
    tile_height = tile->height + tile->depth_half_height.depth;
    /* The tile surface, tile_height * BATTLE_MAP_HEIGHT_STEP, in 20.12. */
    floor = (tile_height * 3) << 14;
    peak = arc_height_term_calculate(query->delta.distance);
    if (query->delta.distance != 0) {
        height = query->target.height / 2;
        limit = (height + query->delta.y) * ONE;
        if (peak >= limit && floor >= limit) {
            query->target.height = height;
        } else {
            height = query->target.height * 3 / 4;
            limit = (height + query->delta.y) * ONE;
            if (peak >= limit && floor >= limit) {
                query->target.height = height;
            } else {
                limit = query->delta.y * ONE;
                if (peak < limit || floor < limit) {
                    return result;
                }
                query->target.height = 0;
            }
        }
        gravity = s_gravity_modifier >> 6;
        distance = query->delta.distance >> 6;
        root = SquareRoot12((gravity - (limit >> 5)) * gravity - distance * distance);
        *arc_a = ((root + s_gravity_modifier) << 8) / (query->delta.distance >> 4);
        *arc_b = ((s_gravity_modifier - root) << 8) / (query->delta.distance >> 4);
        result = 1;
    }
    return result;
}

/*
 * Evaluate the arcing-weapon trajectory term a * b - (1 + a^2) * b^2 /
 * (2 * g_gravity_modifier) in 20.12 fixed point.
 *
 * This is the height of a ballistic arc after a horizontal distance b
 * (`progress`), where a (`launch_slope_q12`) is the tangent of the launch pitch:
 * the callers take the pitch itself as ratan2(launch_slope_q12, ONE).
 *
 * Both operands are pre-shifted by range so the squares stay within 32 bits.
 * g_gravity_modifier is the trajectory gravity modifier
 * (0xa8000).
 */
s32 projectile_arc_trajectory_term_calculate(s32 launch_slope_q12, s32 progress) {
    s32 divisor = (s_gravity_modifier * 2) >> 12;
    s32 slope_sq;
    s32 progress_sq;
    s32 linear;

    if (launch_slope_q12 >= -0x7fff && launch_slope_q12 <= 0x7fff) {
        slope_sq = (launch_slope_q12 * launch_slope_q12) >> 12;
        if (progress >= -0xfffff && progress <= 0xfffff) {
            progress_sq = (progress >> 5) * (progress >> 5);
            progress_sq >>= 2;
            linear = (launch_slope_q12 * (progress >> 5)) >> 7;
        } else {
            progress_sq = (progress >> 7) * (progress >> 7);
            progress_sq <<= 2;
            linear = (launch_slope_q12 * (progress >> 7)) >> 5;
        }
    } else {
        launch_slope_q12 >>= 4;
        slope_sq = (launch_slope_q12 * launch_slope_q12) >> 4;
        if (progress >= -0xfffff && progress <= 0xfffff) {
            progress_sq = (progress >> 5) * (progress >> 5);
            progress_sq >>= 2;
            linear = (launch_slope_q12 * (progress >> 5)) >> 3;
        } else {
            progress_sq = (progress >> 7) * (progress >> 7);
            progress_sq <<= 2;
            linear = (launch_slope_q12 * (progress >> 7)) >> 1;
        }
    }
    return linear - (((slope_sq + ONE) >> 6) * (progress_sq >> 6)) / divisor;
}

/* Offset of a point current along a straight path of length total: dst is src,
 * the whole path's delta, scaled by current / total (a Q12 fraction). */
void projectile_trajectory_position_calculate(const VECTOR* src, s32 total, s32 current, VECTOR* dst) {
    s32 progress = (current << 8) / (total >> 4);

    dst->vx = (progress * src->vx) >> 12;
    dst->vy = (progress * src->vy) >> 12;
    dst->vz = (progress * src->vz) >> 12;
}

/* Launch speed of every arcing projectile: SquareRoot12 of g_gravity_modifier times
 * g_gravity_scale. With the retail values (0xa8000 and 0x53a) it is a constant,
 * about 7.4 in Q12. */
s32 projectile_arc_launch_velocity_calculate(void) {
    return SquareRoot12((s_gravity_modifier * s_gravity_scale) >> 12);
}

/*
 * List every present unit other than `first` and `second` that is not Jumping
 * and is not a mount carrying a rider, with its tile center and height and its
 * sprite height, into a record at the start of the scratchpad returned via `out`.
 */
static void obstacle_units_list(s32 first, s32 second, obstacle_unit_list_t** out) {
    SVECTOR position;
    unit_stats_t* unit;
    s32 i;
    s32 slot;

    *out = (obstacle_unit_list_t*)getScratchAddr(0);
    (*out)->count = 0;
    for (i = 0; i < UNIT_SLOT_COUNT; i++) {
        unit = unit_stats_get(i);
        if (unit->active_battle_id != first && unit->active_battle_id != second
            && unit->active_battle_id != UNIT_ACTIVE_BATTLE_ID_NONE
            && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_JUMP)] & STATUS_BYTE_MASK(STATUS_ID_JUMP))
            && !(unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT)) {
            slot = (*out)->count++;
            (*out)->battle_ids[slot] = unit->active_battle_id;
            unit_battle_tile_center_height_get(unit->active_battle_id, &position);
            (*out)->positions[slot] = position;
            (*out)->sprite_heights[slot] = sprite_battle_sheet_height_get(unit->active_battle_id);
        }
    }
}

/*
 * Test a projectile position against the terrain and the listed units.
 *
 * Fills `tile_ref` with the map tile under `position` and returns 1 when the
 * position is inside that tile's solid column (either map layer) or inside a
 * listed unit's sprite box; the blocking unit's battle id is reported through
 * `out_unit`, which is otherwise left at -1.
 */
static s32 position_for_obstacle_test(
    const obstacle_unit_list_t* list, const VECTOR* position, s32* out_unit, battle_map_tile_coordinates_t* tile_ref) {
    battle_map_tile_t* tile;
    s32 ceiling;
    s32 height;
    s32 y;
    s32 i;

    *out_unit = -1;
    tile_ref->x = position->vx / BATTLE_MAP_TILE_SIZE;
    tile_ref->y = position->vz / BATTLE_MAP_TILE_SIZE;
    tile = battle_map_tile_get_pointer(tile_ref->x, tile_ref->y, 0);
    if (tile->ceiling_depth_and_marks.ceiling_depth == 0) {
        ceiling = tile->height;
    } else {
        ceiling = tile->ceiling_depth_and_marks.ceiling_depth;
    }
    height = tile->height;
    y = position->vy;
    if (y >= -(height * BATTLE_MAP_HEIGHT_STEP) && y < -((height - ceiling) * BATTLE_MAP_HEIGHT_STEP)) {
        tile_ref->layer = 0;
        g_projectile_tile_flags = tile->flags._unknown_00;
        return 1;
    }

    tile = battle_map_tile_get_pointer(tile_ref->x, tile_ref->y, 1);
    ceiling = 1;
    if (tile->ceiling_depth_and_marks.ceiling_depth != 0) {
        ceiling = tile->ceiling_depth_and_marks.ceiling_depth;
    }
    height = tile->height;
    y = position->vy;
    if (y >= -(height * BATTLE_MAP_HEIGHT_STEP) && y < -((height - ceiling) * BATTLE_MAP_HEIGHT_STEP)) {
        tile_ref->layer = 1;
        g_projectile_tile_flags = tile->flags._unknown_00;
        return 1;
    }

    for (i = 0; i < list->count; i++) {
        if (position->vy < list->positions[i].vy && list->positions[i].vy - list->sprite_heights[i] < position->vy
            && list->sprite_heights[i] != 0 && list->positions[i].vx - 8 < position->vx
            && position->vx < list->positions[i].vx + 8 && list->positions[i].vz - 8 < position->vz
            && position->vz < list->positions[i].vz + 8) {
            *out_unit = list->battle_ids[i];
            return 1;
        }
    }
    return 0;
}

/*
 * Step a lobbed projectile along a ballistic arc. The launch pitch comes from
 * ratan2(launch_slope_q12, ONE) and is published in both the arc descriptor and
 * g_projectile_arctan_angle_mod; the per-step advance is the horizontal
 * component of the gravity impulse. Each step rotates the local (progress,
 * -trajectory term) offset into world space through the yaw matrix and tests
 * the result against the obstacle list, exactly as
 * direct_path_trace does for straight shots. On a clear
 * path g_projectile_step_count holds the full step count and the result is 1; on a hit it
 * holds the steps taken and the result is 0.
 *
 * `hit` is deliberately left uninitialized: the target reads $s6 after a
 * zero-step loop without ever writing it.
 */
static s32 arc_path_trace(const SVECTOR* origin, const s32* launch_slope_q12, const s32* distance, SVECTOR* arc,
    const obstacle_unit_list_t* obstacles) {
    SVECTOR rotation;
    SVECTOR step;
    VECTOR position;
    VECTOR previous;
    MATRIX matrix;
    s32 angle;
    s32 velocity;
    s32 advance;
    s32 steps;
    s32 progress;
    s32 term;
    s32 hit;
    s32 i;

    angle = ratan2(*launch_slope_q12, ONE);
    arc->vz = angle;
    s_projectile_arctan_angle_mod = angle;
    progress = 0;
    velocity = projectile_arc_launch_velocity_calculate() >> 8;
    advance = (rcos(arc->vz) * velocity) >> 4;
    if (advance != 0) {
        steps = ((advance + *distance) - 1) / advance;
    } else {
        system_report_stub(1, 0);
        steps = 0;
    }

    rotation.vz = 0;
    rotation.vx = 0;
    rotation.vy = arc->vy;
    RotMatrix(&rotation, &matrix);
    position.vx = matrix.t[0] = origin->vx;
    i = 0;
    position.vy = matrix.t[1] = origin->vy;
    position.vz = matrix.t[2] = origin->vz;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    step.vz = 0;
    for (; i < steps; i++) {
        previous = position;
        progress += advance;
        term = projectile_arc_trajectory_term_calculate(*launch_slope_q12, progress);
        step.vx = progress >> 12;
        step.vy = -term >> 12;
        RotTrans(&step, &position, &position.pad);
        hit = position_for_obstacle_test(obstacles, &position, &g_projectile_hit_unit_id, &g_projectile_tile);
        if (hit != 0) {
            break;
        }
    }

    setVector(&g_projectile_step, position.vx - previous.vx, position.vy - previous.vy, position.vz - previous.vz);
    copyVector(&g_projectile_position, &position);
    if (hit != 0) {
        g_projectile_step_count = i;
        return 0;
    }
    g_projectile_step_count = steps;
    return 1;
}

/* Test whether either arcing projectile path reaches the target unit.
 *
 * The arc counterpart of projectile_direct_check_between_units: the
 * source is raised by two thirds of its sprite height, arc_trajectory_calculate yields
 * the two candidate arcs for the offset, and each is traced against the
 * other units' positions. Returns 1 when an arc is clear. */
s32 projectile_arc_check_between_units(u8 source_id, u8 target_id) {
    SVECTOR rotation;
    arc_query_t query;
    obstacle_unit_list_t* obstacles;
    s32 arc_a;
    s32 arc_b;

    query.source.unit_id = source_id;
    query.target.unit_id = target_id;
    query.source.height = sprite_battle_sheet_height_get(source_id) * 2 / 3;
    query.target.height = sprite_battle_sheet_height_get(target_id);
    obstacle_units_list(source_id, target_id, &obstacles);
    unit_battle_tile_center_height_get(source_id, &query.source.position);
    query.source.position.vy -= query.source.height;
    unit_battle_tile_center_height_get(target_id, &query.target.position);
    query.delta.x = query.target.position.vx - query.source.position.vx;
    query.delta.z = query.target.position.vz - query.source.position.vz;
    query.delta.y = query.source.position.vy - query.target.position.vy;
    query.delta.distance = SquareRoot12((query.delta.x * query.delta.x + query.delta.z * query.delta.z) * ONE);
    rotation.vy = ratan2(-query.delta.z, query.delta.x);
    if (arc_trajectory_calculate(&query, &arc_a, &arc_b) != 0
        && (arc_path_trace(&query.source.position, &arc_a, &query.delta.distance, &rotation, obstacles) != 0
            || arc_path_trace(&query.source.position, &arc_b, &query.delta.distance, &rotation, obstacles) != 0)) {
        return 1;
    }
    return 0;
}

/*
 * Set up an arcing projectile from source_id toward target_id (or the tile at
 * tile_position when target_id is -1) and pick the arc to fly.
 *
 * Trace arc_b first, then arc_a if it is blocked; a clear arc returns target_id.
 * Otherwise an arc blocked by a unit of another team than the source wins
 * (arc_b's first) and returns
 * that unit. Failing that, arc_a's trace is kept and its blocker (or -1) is
 * returned, except when only arc_a was blocked by a unit: then arc_b's
 * terrain-blocked result is restored and -1 returned. g_projectile_launch_slope_q12 receives the
 * chosen launch slope. `unused` reserves the target's 16 otherwise unused frame bytes.
 */
s32 projectile_arc_set_and_validate(s32 source_id, const SVECTOR* tile_position, s32 target_id) {
    s32 unused[4];
    SVECTOR arc;
    VECTOR second_step;
    VECTOR first_step;
    SVECTOR second_position;
    SVECTOR first_position;
    arc_query_t query;
    obstacle_unit_list_t* obstacles;
    s32 arc_a;
    s32 arc_b;
    battle_map_tile_t* tile;
    unit_stats_t* stats;
    unit_stats_t* other;
    s32 hit;
    s32 second_hit;
    s32 first_unit;
    s32 first_other;
    s32 first_count;
    s32 second_unit;
    s32 second_other;
    s32 second_count;
    s32 result;
    s32 launch_slope_q12;

    g_projectile_source_id = source_id;
    query.source.unit_id = source_id;
    query.target.unit_id = target_id;
    query.source.height = sprite_battle_sheet_height_get(source_id) * 2 / 3;
    if (target_id != -1) {
        query.target.height = sprite_battle_sheet_height_get(target_id);
    } else {
        query.target.height = 0;
    }
    obstacle_units_list(source_id, -1, &obstacles);
    unit_battle_tile_center_height_get(source_id, &g_projectile_origin);
    g_projectile_origin.vy -= query.source.height;
    if (target_id == -1) {
        tile = battle_map_tile_get_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_projectile_destination.vx = tile_position->vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        g_projectile_destination.vz = tile_position->vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        g_projectile_destination.vy = -((tile->height + tile->depth_half_height.depth) * BATTLE_MAP_HEIGHT_STEP);
    } else {
        unit_battle_tile_center_height_get(target_id, &g_projectile_destination);
    }
    query.delta.x = g_projectile_destination.vx - g_projectile_origin.vx;
    query.delta.z = g_projectile_destination.vz - g_projectile_origin.vz;
    query.delta.y = g_projectile_origin.vy - g_projectile_destination.vy;
    query.delta.distance = SquareRoot12((query.delta.x * query.delta.x + query.delta.z * query.delta.z) * ONE);
    arc.vy = ratan2(-query.delta.z, query.delta.x);
    if (arc_trajectory_calculate(&query, &arc_a, &arc_b) == 0) {
        arc_b = ONE;
        arc_a = ONE;
    }

    hit = arc_path_trace(&g_projectile_origin, &arc_b, &query.delta.distance, &arc, obstacles);
    first_unit = g_projectile_hit_unit_id;
    first_step = g_projectile_step;
    first_position = g_projectile_position;
    first_count = g_projectile_step_count;
    first_other = -1;
    if (g_projectile_hit_unit_id == -1) {
        first_other = g_projectile_tile_flags;
    }
    if (hit != 0) {
        launch_slope_q12 = arc_b;
        result = target_id;
    } else {
        second_hit = arc_path_trace(&g_projectile_origin, &arc_a, &query.delta.distance, &arc, obstacles);
        second_unit = g_projectile_hit_unit_id;
        second_step = g_projectile_step;
        second_position = g_projectile_position;
        second_count = g_projectile_step_count;
        second_other = -1;
        if (g_projectile_hit_unit_id == -1) {
            second_other = g_projectile_tile_flags;
        }
        if (second_hit != 0) {
            launch_slope_q12 = arc_a;
            result = target_id;
        } else {
            stats = unit_stats_get(source_id);
            if (first_unit != -1
                && (other = unit_stats_get(first_unit),
                    (stats->team_flags & UNIT_TEAM_MASK) != (other->initial_team_flags & UNIT_TEAM_MASK))) {
                g_projectile_step = first_step;
                g_projectile_position = first_position;
                launch_slope_q12 = arc_b;
                g_projectile_hit_unit_id = first_unit;
                result = first_unit;
                g_projectile_step_count = first_count;
            } else if (second_unit != -1
                && (other = unit_stats_get(second_unit),
                    (stats->team_flags & UNIT_TEAM_MASK) != (other->initial_team_flags & UNIT_TEAM_MASK))) {
                launch_slope_q12 = arc_a;
                g_projectile_step_count = second_count;
                g_projectile_hit_unit_id = second_unit;
                result = second_unit;
            } else if (second_unit == -1) {
                launch_slope_q12 = arc_a;
                g_projectile_step_count = second_count;
                g_projectile_hit_unit_id = second_unit;
                g_projectile_tile_flags = second_other;
                result = -1;
            } else if (first_unit != -1) {
                launch_slope_q12 = arc_a;
                g_projectile_step_count = second_count;
                g_projectile_tile_flags = second_other;
                g_projectile_hit_unit_id = second_unit;
                result = second_unit;
            } else {
                g_projectile_step = first_step;
                g_projectile_position = first_position;
                launch_slope_q12 = arc_b;
                g_projectile_hit_unit_id = first_unit;
                result = -1;
                g_projectile_tile_flags = first_other;
                g_projectile_step_count = first_count;
            }
        }
    }
    g_projectile_launch_slope_q12 = launch_slope_q12;
    return result;
}

/* Choose the bow animation column from the last trajectory test's angle.
 * Wrap g_projectile_arctan_angle_mod into [-0x800, 0x800]: return column 0
 * below -0x2aa, column 1 through 0x2aa, or column 2 above it. Arc selection
 * can restore an earlier path without restoring this angle. */
s32 projectile_bow_animation_column_get(void) {
    s16 angle = s_projectile_arctan_angle_mod;
    if (angle >= 0x801) {
        s_projectile_arctan_angle_mod = angle - ONE;
    }
    {
        s16 angle_after_hi = s_projectile_arctan_angle_mod;
        if (angle_after_hi < -0x800) {
            s_projectile_arctan_angle_mod = angle_after_hi + ONE;
        }
    }
    {
        s16 wrapped = s_projectile_arctan_angle_mod;
        if (wrapped < 0x2AB) {
            return wrapped >= -0x2AA;
        }
        return 2;
    }
}

/*
 * Step a projectile from `origin` along `delta` in 0x2000 increments of the
 * `distance` total, testing each position against the obstacle list. Stores
 * the last step's movement and the final position, sets g_projectile_step_count to a
 * quarter of the steps taken, and returns 1 when the path is clear or 0 when
 * an obstacle stopped it. The `step` variable keeps the target's register
 * divide; a literal 0x2000 folds to a shift.
 */
static s32 direct_path_trace(
    const VECTOR* delta, const SVECTOR* origin, const s32* distance, const obstacle_unit_list_t* obstacles) {
    VECTOR position;
    VECTOR previous;
    s32 step;
    s32 steps;
    s32 progress;
    s32 hit;
    s32 i;

    step = 0x2000;
    steps = *distance / step;
    progress = 0;
    position.vx = origin->vx;
    hit = 0;
    position.vy = origin->vy;
    i = 0;
    position.vz = origin->vz;
    for (; i < steps; i++) {
        previous = position;
        progress += step;
        projectile_trajectory_position_calculate(delta, *distance, progress, &position);
        position.vx += origin->vx;
        position.vy += origin->vy;
        position.vz += origin->vz;
        hit = position_for_obstacle_test(obstacles, &position, &g_projectile_hit_unit_id, &g_projectile_tile);
        if (hit != 0) {
            break;
        }
    }
    setVector(&g_projectile_step, position.vx - previous.vx, position.vy - previous.vy, position.vz - previous.vz);
    copyVector(&g_projectile_position, &position);
    if (hit != 0) {
        g_projectile_step_count = i / 4;
        return 0;
    }
    g_projectile_step_count = steps / 4;
    return 1;
}

/*
 * Trace a projectile path from one unit's upper body to another's.
 *
 * Both endpoints are raised by two thirds of the unit's sprite height (12 when
 * the target has none). `unused_20` and `unused_38` reproduce unreferenced
 * frame slots the target reserves between the vectors.
 */
s32 projectile_direct_check_between_units(u8 source_id, u8 target_id) {
    SVECTOR origin;
    SVECTOR destination;
    SVECTOR unused_20;
    VECTOR delta;
    s32 unused_38[2];
    obstacle_unit_list_t* obstacles;
    s32 distance;
    s32 source_height;
    s32 target_height;

    source_height = sprite_battle_sheet_height_get(source_id) * 2 / 3;
    target_height = sprite_battle_sheet_height_get(target_id) * 2 / 3;
    if (target_height == 0) {
        target_height = 12;
    }
    obstacle_units_list(source_id, target_id, &obstacles);
    unit_battle_tile_center_height_get(source_id, &origin);
    origin.vy -= source_height;
    unit_battle_tile_center_height_get(target_id, &destination);
    destination.vy -= target_height;
    setVector(&delta, destination.vx - origin.vx, destination.vy - origin.vy, destination.vz - origin.vz);
    distance = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
    return direct_path_trace(&delta, &origin, &distance, obstacles);
}

/*
 * Traces a straight projectile path from a source unit to either a target
 * unit or a bare tile, and reports whether the flight is obstructed.
 *
 * Both endpoints are raised by two thirds of their sprite height (12 when the
 * target has none), as in projectile_direct_check_between_units;
 * the tile case instead centers on the tile at tile_position (x, layer, y) and
 * sits on its surface, as in projectile_arrow_trajectory_set.  The extra
 * work over those two is the launch angle: ratan2 of the vertical delta
 * against the horizontal distance, biased by 0x400 (a quarter turn) into
 * g_projectile_arctan_angle_mod.  Returns target_id when the path is clear
 * and the blocking unit from g_projectile_hit_unit_id when it is not.
 */
s32 projectile_direct_check_to_target(s32 source_id, const SVECTOR* tile_position, s32 target_id) {
    VECTOR delta;
    battle_map_tile_t* tile;
    obstacle_unit_list_t* obstacles;
    s32 distance;
    s32 source_height;
    s32 target_height;

    g_projectile_source_id = source_id;
    source_height = sprite_battle_sheet_height_get(source_id) * 2 / 3;
    if (target_id != -1) {
        target_height = sprite_battle_sheet_height_get(target_id) * 2 / 3;
        if (target_height == 0) {
            target_height = 12;
        }
    }
    obstacle_units_list(source_id, -1, &obstacles);
    unit_battle_tile_center_height_get(source_id, &g_projectile_origin);
    g_projectile_origin.vy -= source_height;
    if (target_id == -1) {
        tile = battle_map_tile_get_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_projectile_destination.vx = tile_position->vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        g_projectile_destination.vz = tile_position->vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        g_projectile_destination.vy = -(tile->height * BATTLE_MAP_HEIGHT_STEP);
    } else {
        unit_battle_tile_center_height_get(target_id, &g_projectile_destination);
        g_projectile_destination.vy -= target_height;
    }
    setVector(&delta, g_projectile_destination.vx - g_projectile_origin.vx,
        g_projectile_destination.vy - g_projectile_origin.vy, g_projectile_destination.vz - g_projectile_origin.vz);
    s_projectile_arctan_angle_mod = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
    distance = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
    if (direct_path_trace(&delta, &g_projectile_origin, &distance, obstacles) != 0) {
        return target_id;
    }
    return g_projectile_hit_unit_id;
}

/* Sets an arrow's source, destination and flight duration.
 *
 * The projectile origin is the first unit raised by two thirds of its
 * sprite height, and the destination is the second unit raised by three
 * quarters of its height, or, when target_id is -1, to the center of the tile
 * at tile_position (x, layer, y) on its surface. g_projectile_step_count receives the
 * flight duration derived from the distance, as in
 * projectile_direct_check_between_units / direct_path_trace.
 * `delta` reproduces the target's stores to an otherwise unused stack vector.
 */
void projectile_arrow_trajectory_set(s32 source_id, const SVECTOR* tile_position, s32 target_id) {
    VECTOR delta;
    battle_map_tile_t* tile;
    s32 source_height = sprite_battle_sheet_height_get(source_id) * 2 / 3;
    s32 target_height;

    if (target_id != -1) {
        target_height = sprite_battle_sheet_height_get(target_id) * 3 / 4;
    }
    unit_battle_tile_center_height_get(source_id, &g_projectile_origin);
    g_projectile_origin.vy -= source_height;
    if (target_id == -1) {
        tile = battle_map_tile_get_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_projectile_destination.vx = tile_position->vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        g_projectile_destination.vz = tile_position->vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
        g_projectile_destination.vy = -(tile->height * BATTLE_MAP_HEIGHT_STEP);
    } else {
        unit_battle_tile_center_height_get(target_id, &g_projectile_destination);
        g_projectile_destination.vy -= target_height;
    }
    setVector(&delta, g_projectile_destination.vx - g_projectile_origin.vx,
        g_projectile_destination.vy - g_projectile_origin.vy, g_projectile_destination.vz - g_projectile_origin.vz);
    g_projectile_step_count
        = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE) / 8 >> 12;
}
