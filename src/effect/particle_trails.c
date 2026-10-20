/* Unseen particles running from a spawn volume to a tip, spawning child effects from the ends of their trails. */

#include "fft/battle_map.h"
#include "fft/effect.h"
#include "fft/effect_emitter_data.h"
#include "fft/effect_file.h"
#include "fft/effect_heap.h"
#include "fft/effect_math.h"
#include "fft/effect_particle.h"
#include "fft/effect_script.h"
#include "fft/random.h"
#include "fft/unit_runtime.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* A 17-point particle trail; spawning seeds only points 0-8 (the target's loop bound is 9). */
typedef struct particle_trail {
    SVECTOR tip;          /* 0x00 */
    VECTOR velocity;      /* 0x08: 20.12 fixed */
    VECTOR position;      /* 0x18: 20.12 fixed */
    vec3s16_t points[17]; /* 0x28 */
    s16 age;              /* 0x8e */
    s16 travel_duration;  /* 0x90 */
    s16 active;           /* 0x92 */
    u8 _unused_94[0x04];
} particle_trail_t;
typedef char particle_trail_size_must_be_152[sizeof(particle_trail_t) == 152 ? 1 : -1];

typedef struct effect_trail_work {
    s32 buffer;                 /* 0x000 */
    s16 count;                  /* 0x004 */
    u8 _padding_006[0x02];      /* aligns trails */
    particle_trail_t trails[4]; /* 0x008 */
    MATRIX matrix;              /* 0x268 */
    s32 destroy_delay;          /* 0x288 */
} effect_trail_work_t;
typedef char effect_trail_work_size_must_be_652[sizeof(effect_trail_work_t) == 652 ? 1 : -1];

/* Runs up to four unseen particles from the emitter's spawn volume towards a
 * tip, keeping a 17-point trail of each, and spawns the emitter's children
 * from the trail's head (the newest point while traveling, the arrival point
 * after): the mid-life child every frame and the death child when the trail
 * has run out. The tentacle mesh renderer without its mesh. */
s32 effect_particle_trails_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame) {
    s16 state_index = record_index;
    SVECTOR tip;
    SVECTOR tip_spread;
    VECTOR origin;
    VECTOR spread;
    VECTOR base_angles;
    VECTOR direction_spread;
    SVECTOR caster;
    SVECTOR target;
    SVECTOR vector;
    VECTOR out;
    SVECTOR tip_map_max;
    MATRIX facing;
    effect_work_record_t parent;
    SVECTOR map_max;
    effect_record_t* record = &g_effect_state_records[state_index];
    effect_trail_work_t* work;
    const effect_geometry_record_t* entry = &g_effect_geometry_table->entries[emitter_index];
    s16 speed_min;
    s16 speed_max;
    s16 travel_duration_min;
    s16 travel_duration_max;
    s32 i;
    s32 count;
    s32 j;
    s32 remaining_trail_steps;
    particle_trail_t* trail;
    battle_map_tile_t* tile;
    s32 orient;
    u16 placement = entry->tentacle_mesh.placement_flags;
    u16 flags = entry->tentacle_mesh.flags;
    s32 target_index;
    s32 spawn_bindings;
    s32 tip_bindings;
    s32 index;
    s32 factor;
    s16 largest;
    s32 speed;
    s32 point;
    effect_target_t* caster_target;
    s32 px;
    s32 tail_step;
    s32 x;
    s32 y;
    s32 z;
    s16 facing_yaw;
    s32 py;
    s32 pz;
    s32 wrap;

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;

    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            effect_heap_free_block(record->work_slots[callback_slot]);
        }
        work = record->work_slots[callback_slot] = effect_heap_alloc_block(sizeof(effect_trail_work_t), state_index);

        for (index = 3; index >= 0; index--) {
            work->trails[index].active = 0;
        }

        work->buffer = 0;
        work->count = 0;
        work->destroy_delay = 0;
        work->matrix.t[2] = 0;
        work->matrix.t[1] = 0;
        work->matrix.t[0] = 0;
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;

    case EFFECT_CALLBACK_PHASE_UPDATE:
        /* i keeps the unwrapped frame for the spawn interval; the curves wrap the frame. */
        i = frame - 1;
        work = record->work_slots[callback_slot];
        tip_bindings = entry->tentacle_mesh.tip_curve_bindings;
        target_index = record->timeline.target_index;
        spawn_bindings = entry->tentacle_mesh.spawn_curve_bindings;
        frame %= EFFECT_ANIMATION_CURVE_FRAMES;

        /* Spawn a batch every spawn interval, starting at frame 1. */
        EFFECT_READ_CURVE(index, factor, (tip_bindings >> EFFECT_EMITTER_CURVE_SPAWN_INTERVAL_SHIFT) & 0xf, frame);
        if (i
                % effect_lerp_q8(
                    entry->tentacle_mesh.spawn_interval_start, entry->tentacle_mesh.spawn_interval_end, factor)
            == 0) {
            /* work->matrix keeps a zero translation, so the spawn RotTrans calls only rotate. */
            SetTransMatrix(&work->matrix);
            orient = (flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING;

            /* The caster's facing as (quadrant + 1) quarter turns of yaw; left unset for a non-unit caster. */
            caster_target = &g_effect_context.targets[EFFECT_CASTER_ENTRY];
            if (caster_target->target_type == EFFECT_TARGET_TYPE_UNIT) {
                facing_yaw = ((unit_runtime_facing_quadrant_get(caster_target->data.unit.runtime_id) + 1) & 3) << 10;
                vector.vz = 0;
                vector.vx = 0;
                vector.vy = facing_yaw;
                RotMatrix(&vector, &facing);
            }

            effect_target_world_coords_resolve(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
                EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &caster);
            effect_target_world_coords_resolve(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
                target_index, &g_effect_context, &target);

            EFFECT_READ_CURVE(index, factor, (tip_bindings >> EFFECT_EMITTER_CURVE_PARTICLE_COUNT_SHIFT) & 0xf, frame);
            count = effect_lerp_q8(
                entry->tentacle_mesh.particle_count_start, entry->tentacle_mesh.particle_count_end, factor);

            EFFECT_READ_CURVE(index, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_SPAWN_POSITION_SHIFT) & 0xf, frame);
            effect_emitter_spawn_position_interpolate(entry, factor, &origin);
            switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
            case EFFECT_EMITTER_ORIGIN_WORLD:
            case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
                break;
            case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
                tile = battle_map_tile_get_pointer(
                    g_effect_context.target_tile.vx, g_effect_context.target_tile.vz, g_effect_context.target_tile.vy);
                origin.vx += g_effect_context.target_tile.vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
                origin.vz += g_effect_context.target_tile.vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
                origin.vy -= tile->height * BATTLE_MAP_HEIGHT_STEP;
                break;
            case EFFECT_EMITTER_ORIGIN_CASTER:
                origin.vx += caster.vx;
                origin.vy += caster.vy;
                origin.vz += caster.vz;
                break;
            case EFFECT_EMITTER_ORIGIN_TARGET:
                origin.vx += target.vx;
                origin.vy += target.vy;
                origin.vz += target.vz;
                break;
            case EFFECT_EMITTER_ORIGIN_MAP_CENTER:
                battle_map_dimensions_store(&map_max);
                origin.vx += map_max.vx * (BATTLE_MAP_TILE_SIZE / 2);
                origin.vz += map_max.vz * (BATTLE_MAP_TILE_SIZE / 2);
                break;
            }

            EFFECT_READ_CURVE(index, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_SPAWN_SPREAD_SHIFT) & 0xf, frame);
            effect_emitter_spawn_spread_interpolate(entry, factor, &spread);
            if (orient) {
                SetRotMatrix(&facing);
                vector.vx = spread.vx;
                vector.vy = spread.vy;
                vector.vz = spread.vz;
                RotTrans(&vector, &spread, &out.pad);
            }

            index = (spawn_bindings >> EFFECT_SPAWN_CURVE_RADIAL_SPEED_SHIFT) & 0xf;
            index--;
            if (index != -1) {
                factor = g_effect_animation_curve_data->curves[index][frame];
                speed_min = effect_lerp_q8(
                    entry->tentacle_mesh.radial_speed_start.min, entry->tentacle_mesh.radial_speed_end.min, factor);
                speed_max = effect_lerp_q8(
                    entry->tentacle_mesh.radial_speed_start.max, entry->tentacle_mesh.radial_speed_end.max, factor);
            } else {
                speed_min = entry->tentacle_mesh.radial_speed_start.min;
                speed_max = entry->tentacle_mesh.radial_speed_start.max;
            }

            EFFECT_READ_CURVE(
                index, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_VELOCITY_DIRECTION_SPREAD_SHIFT) & 0xf, frame);
            effect_emitter_velocity_spread_interpolate(entry, factor, &direction_spread);
            EFFECT_READ_CURVE(
                index, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_VELOCITY_BASE_ANGLE_SHIFT) & 0xf, frame);
            effect_emitter_velocity_angles_interpolate(entry, factor, &base_angles);

            EFFECT_READ_CURVE(index, factor, (tip_bindings >> EFFECT_TENTACLE_CURVE_TIP_OFFSET_SHIFT) & 0xf, frame);
            tip.vx = effect_lerp_q8(
                entry->tentacle_mesh.target_offset_start.x, entry->tentacle_mesh.target_offset_end.x, factor);
            tip.vy = effect_lerp_q8(
                entry->tentacle_mesh.target_offset_start.y, entry->tentacle_mesh.target_offset_end.y, factor);
            tip.vz = effect_lerp_q8(
                entry->tentacle_mesh.target_offset_start.z, entry->tentacle_mesh.target_offset_end.z, factor);
            switch (placement & EFFECT_EMITTER_HOMING_MASK) {
            case EFFECT_EMITTER_HOMING_OFFSET:
            case EFFECT_EMITTER_HOMING_OFFSET_ALT:
                break;
            case EFFECT_EMITTER_HOMING_MAP_CENTER:
                battle_map_dimensions_store(&tip_map_max);
                tip.vx += tip_map_max.vx * (BATTLE_MAP_TILE_SIZE / 2);
                tip.vz += tip_map_max.vz * (BATTLE_MAP_TILE_SIZE / 2);
                break;
            case EFFECT_EMITTER_HOMING_CASTER:
                tip.vx += caster.vx;
                tip.vy += caster.vy;
                tip.vz += caster.vz;
                break;
            case EFFECT_EMITTER_HOMING_TARGET:
                tip.vx += target.vx;
                tip.vy += target.vy;
                tip.vz += target.vz;
                break;
            case EFFECT_EMITTER_HOMING_CURSOR_TILE:
                tile = battle_map_tile_get_pointer(
                    g_effect_context.target_tile.vx, g_effect_context.target_tile.vz, g_effect_context.target_tile.vy);
                tip.vx += g_effect_context.target_tile.vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
                tip.vz += g_effect_context.target_tile.vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
                tip.vy -= tile->height * BATTLE_MAP_HEIGHT_STEP;
                break;
            }

            EFFECT_READ_CURVE(index, factor, (tip_bindings >> EFFECT_TENTACLE_CURVE_TIP_SPREAD_SHIFT) & 0xf, frame);
            tip_spread.vx = effect_lerp_q8(
                entry->tentacle_mesh.tip_spread_start_x, entry->tentacle_mesh.tip_spread_end_x, factor);
            tip_spread.vy = effect_lerp_q8(
                entry->tentacle_mesh.tip_spread_start_y, entry->tentacle_mesh.tip_spread_end_y, factor);
            tip_spread.vz = effect_lerp_q8(
                entry->tentacle_mesh.tip_spread_start_z, entry->tentacle_mesh.tip_spread_end_z, factor);

            EFFECT_READ_CURVE(
                index, factor, (tip_bindings >> EFFECT_TENTACLE_CURVE_TRAVEL_DURATION_SHIFT) & 0xf, frame);
            travel_duration_min = effect_lerp_q8(
                entry->tentacle_mesh.travel_duration_start.min, entry->tentacle_mesh.travel_duration_end.min, factor);
            travel_duration_max = effect_lerp_q8(
                entry->tentacle_mesh.travel_duration_start.max, entry->tentacle_mesh.travel_duration_end.max, factor);

            /* Start up to count particles while free slots remain. */
            for (i = 0; i < count; i++) {
                if (work->count >= 4) {
                    break;
                }
                work->count++;
                for (j = 0; j < 4; j++) {
                    if (work->trails[j].active == 0) {
                        trail = &work->trails[j];
                        break;
                    }
                }

                /* A random direction scaled by the spread on each axis: a point on the spread ellipsoid. */
                largest = effect_s16_largest_magnitude_select(spread.vx, spread.vy, spread.vz);
                if (largest != 0) {
                    vector.vx = rand();
                    vector.vy = rand();
                    vector.vz = rand();
                    RotMatrix(&vector, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    vector.vx = largest;
                    vector.vz = 0;
                    vector.vy = 0;
                    gte_RotTrans(&vector, &out, &out.pad);
                    trail->position.vx = ((out.vx * spread.vx) / largest + origin.vx) * ONE;
                    trail->position.vy = ((out.vy * spread.vy) / largest + origin.vy) * ONE;
                    trail->position.vz = ((out.vz * spread.vz) / largest + origin.vz) * ONE;
                } else {
                    trail->position.vx = origin.vx * ONE;
                    trail->position.vy = origin.vy * ONE;
                    trail->position.vz = origin.vz * ONE;
                }

                /* The tip gets the same treatment with its own spread. */
                largest = effect_s16_largest_magnitude_select(tip_spread.vx, tip_spread.vy, tip_spread.vz);
                if (largest != 0) {
                    vector.vx = rand();
                    vector.vy = rand();
                    vector.vz = rand();
                    RotMatrix(&vector, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    vector.vx = largest;
                    vector.vz = 0;
                    vector.vy = 0;
                    RotTrans(&vector, &out, &out.pad);
                    trail->tip.vx = tip.vx + (tip_spread.vx * out.vx) / largest;
                    trail->tip.vy = tip.vy + (tip_spread.vy * out.vy) / largest;
                    trail->tip.vz = tip.vz + (tip_spread.vz * out.vz) / largest;
                } else {
                    trail->tip = tip;
                }

                /* Speeds are 1/512 unit a frame: << 3, or >> 9 of a 4.12 unit vector, gives 20.12. */
                switch (flags & EFFECT_EMITTER_VELOCITY_MASK) {
                case EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED:
                    vector.vx = base_angles.vx + RANDOM_BETWEEN(direction_spread.vx, -direction_spread.vx);
                    vector.vy = base_angles.vy + RANDOM_BETWEEN(direction_spread.vy, -direction_spread.vy);
                    vector.vz = base_angles.vz + RANDOM_BETWEEN(direction_spread.vz, -direction_spread.vz);
                    RotMatrix(&vector, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    vector.vz = 0;
                    vector.vx = 0;
                    vector.vy = RANDOM_BETWEEN(speed_min, speed_max);
                    RotTrans(&vector, &out, &out.pad);
                    trail->velocity.vx = out.vx << 3;
                    trail->velocity.vy = out.vy << 3;
                    trail->velocity.vz = out.vz << 3;
                    break;
                case EFFECT_EMITTER_VELOCITY_CASTER_FACING:
                    vector.vx = base_angles.vx + RANDOM_BETWEEN(direction_spread.vx, -direction_spread.vx);
                    vector.vy = base_angles.vy + RANDOM_BETWEEN(direction_spread.vy, -direction_spread.vy);
                    vector.vz = base_angles.vz + RANDOM_BETWEEN(direction_spread.vz, -direction_spread.vz);
                    RotMatrix(&vector, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    vector.vz = 0;
                    vector.vx = 0;
                    vector.vy = RANDOM_BETWEEN(speed_min, speed_max);
                    RotTrans(&vector, &out, &out.pad);
                    SetRotMatrix(&facing);
                    vector.vx = out.vx;
                    vector.vy = out.vy;
                    vector.vz = out.vz;
                    RotTrans(&vector, &out, &out.pad);
                    trail->velocity.vx = out.vx << 3;
                    trail->velocity.vy = out.vy << 3;
                    trail->velocity.vz = out.vz << 3;
                    break;
                case EFFECT_EMITTER_VELOCITY_INWARD:
                    out.vx = origin.vx - (trail->position.vx >> 12);
                    out.vy = origin.vy - (trail->position.vy >> 12);
                    out.vz = origin.vz - (trail->position.vz >> 12);
                    speed = RANDOM_BETWEEN(speed_min, speed_max);
                    /* A particle spawned on the origin has no direction to it and moves along +y. */
                    if (out.vx != 0 || out.vy != 0 || out.vz != 0) {
                        VectorNormal(&out, &out);
                        trail->velocity.vx = (speed * out.vx) >> 9;
                        trail->velocity.vy = (speed * out.vy) >> 9;
                        trail->velocity.vz = (speed * out.vz) >> 9;
                    } else {
                        trail->velocity.vy = speed << 3;
                        trail->velocity.vz = 0;
                        trail->velocity.vx = 0;
                    }
                    break;
                }

                trail->travel_duration = RANDOM_BETWEEN(travel_duration_min, travel_duration_max);
                trail->age = 0;
                trail->active = 1;

                /* Only trail points 0-8 of the 17 are seeded on the spawn position (retail loop bound). */
                vector.vx = trail->position.vx >> 12;
                vector.vy = trail->position.vy >> 12;
                vector.vz = trail->position.vz >> 12;
                for (index = 0; index < 9; index++) {
                    trail->points[index].x = vector.vx;
                    trail->points[index].y = vector.vy;
                    trail->points[index].z = vector.vz;
                }
            }
        }

        /* Advance every active particle; nothing is drawn. */
        for (j = 0; j < 4; j++) {
            if (work->trails[j].active == 0) {
                continue;
            }
            trail = &work->trails[j];

            /* While traveling, the newest point eases from the particle's path towards the tip. After
             * arrival, point stays on the point written on the arrival frame. */
            parent.motion.position.x = x = trail->position.vx += trail->velocity.vx;
            parent.motion.position.y = y = trail->position.vy += trail->velocity.vy;
            parent.motion.position.z = z = trail->position.vz += trail->velocity.vz;
            index = ++trail->age;
            point = index % 17;
            px = x >> 12;
            py = y >> 12;
            pz = z >> 12;
            if (index <= trail->travel_duration) {
                trail->points[point].x = effect_s32_cos_interpolate(px, trail->tip.vx, trail->travel_duration, index);
                trail->points[point].y = effect_s32_cos_interpolate(py, trail->tip.vy, trail->travel_duration, index);
                trail->points[point].z = effect_s32_cos_interpolate(pz, trail->tip.vz, trail->travel_duration, index);
            } else {
                trail->points[point].x = trail->tip.vx;
                trail->points[point].y = trail->tip.vy;
                trail->points[point].z = trail->tip.vz;
                tail_step = index - 16;
                remaining_trail_steps = trail->travel_duration - tail_step;
                if (remaining_trail_steps < 0) {
                    remaining_trail_steps = 0;
                }
                wrap = point - 16;
                point = wrap + remaining_trail_steps;
                if (point < 0) {
                    point += 17;
                }
            }

            /* Children spawn from that point. */
            parent.motion.position.x = trail->points[point].x * ONE;
            parent.motion.position.y = trail->points[point].y * ONE;
            parent.motion.position.z = trail->points[point].z * ONE;
            if (flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) {
                effect_emitter_particles_spawn(
                    record->record_index, trail->age, entry->tentacle_mesh.child_emitter_mid_life, &parent);
            }

            /* Done once the trail has run out: the travel time plus a frame per trailing segment. */
            if (trail->travel_duration + 16 == trail->age) {
                if (flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK) {
                    effect_emitter_particles_spawn(
                        record->record_index, trail->age, entry->tentacle_mesh.child_emitter_on_death, &parent);
                }
                trail->active = 0;
                work->count--;
            }
        }

        /* Nothing reads buffer here; the flip is kept from the tentacle renderer. */
        work->buffer = 1 - work->buffer;
        break;

    case EFFECT_CALLBACK_PHASE_DESTROY:
        work = record->work_slots[callback_slot];
        /* Free one call late, as the tentacle renderer does for its quads. The null check
         * below comes after this read through work (retail order). */
        if (work->destroy_delay == 0) {
            work->destroy_delay = 1;
            break;
        }
        if (work != 0) {
            effect_heap_free_block(work);
            record->work_slots[callback_slot] = 0;
        }
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
