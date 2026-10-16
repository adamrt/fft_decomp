/* Unseen particles running from a spawn volume to a tip, spawning child effects from the ends of their trails. */

#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_effect_file.h"
#include "fft/battle_effect_geometry.h"
#include "fft/battle_effect_particle.h"
#include "fft/battle_effect_script.h"
#include "fft/battle_map.h"
#include "fft/battle_unit_animation.h"
#include "fft/effect.h"
#include "fft/map.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Runs up to four unseen particles from the emitter's spawn volume towards a
 * tip, keeping a 17-point trail of each, and spawns the emitter's children
 * from the trail's head (the newest point while travelling, the arrival point
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
    SVECTOR rotation;
    VECTOR out;
    SVECTOR tip_map_max;
    MATRIX facing;
    effect_work_record_t emitter;
    SVECTOR map_max;
    effect_record_t* record = &g_battle_effect_state_records[state_index];
    effect_trail_work_t* work;
    const effect_geometry_record_t* entry = &g_battle_effect_geometry_table->entries[emitter_index];
    s16 speed_min;
    s16 speed_max;
    s16 length_min;
    s16 length_max;
    s32 i;
    s32 count;
    s32 j;
    s32 shade_index;
    effect_trail_segment_t* seg;
    map_tile_t* tile;
    s32 orient;
    u16 placement = entry->tentacle_mesh.placement_flags;
    u16 flags = entry->tentacle_mesh.flags;
    s32 target_index;
    s32 spawn_bindings;
    s32 tip_bindings;
    s32 index;
    s32 factor;
    s16 len;
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
            battle_heap_free_block(record->work_slots[callback_slot]);
        }
        work = record->work_slots[callback_slot] = battle_heap_alloc_block(sizeof(effect_trail_work_t), state_index);

        for (index = 3; index >= 0; index--) {
            work->segments[index].active = 0;
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
                % battle_lerp_q8(
                    entry->tentacle_mesh.spawn_interval_start, entry->tentacle_mesh.spawn_interval_end, factor)
            == 0) {
            /* work->matrix keeps a zero translation, so the spawn RotTrans calls only rotate. */
            SetTransMatrix(&work->matrix);
            orient = (flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING;

            /* The caster's facing as (quadrant + 1) quarter turns of yaw; left unset for a non-unit caster. */
            caster_target = &g_battle_effect_context.targets[EFFECT_CASTER_ENTRY];
            if (caster_target->target_type == EFFECT_TARGET_TYPE_UNIT) {
                facing_yaw
                    = ((battle_unit_get_facing_quadrant_by_runtime_id(caster_target->data.unit.runtime_id) + 1) & 3)
                    << 10;
                rotation.vz = 0;
                rotation.vx = 0;
                rotation.vy = facing_yaw;
                RotMatrix(&rotation, &facing);
            }

            battle_effect_resolve_target_world_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, EFFECT_TARGET_INDEX_CASTER,
                &g_battle_effect_context, &caster);
            battle_effect_resolve_target_world_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, target_index,
                &g_battle_effect_context, &target);

            EFFECT_READ_CURVE(index, factor, (tip_bindings >> EFFECT_EMITTER_CURVE_PARTICLE_COUNT_SHIFT) & 0xf, frame);
            count = battle_lerp_q8(
                entry->tentacle_mesh.particle_count_start, entry->tentacle_mesh.particle_count_end, factor);

            EFFECT_READ_CURVE(index, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_SPAWN_POSITION_SHIFT) & 0xf, frame);
            battle_effect_interpolate_emitter_spawn_position(entry, factor, &origin);
            switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
            case EFFECT_EMITTER_ORIGIN_WORLD:
            case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
                break;
            case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
                tile = battle_map_get_tile_data_pointer(g_battle_effect_target_tile.data.tile.vx,
                    g_battle_effect_target_tile.data.tile.vz, g_battle_effect_target_tile.data.tile.vy);
                origin.vx += g_battle_effect_target_tile.data.tile.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                origin.vz += g_battle_effect_target_tile.data.tile.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                origin.vy -= tile->height * MAP_HEIGHT_STEP;
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
                battle_map_store_max_coordinates(&map_max);
                origin.vx += map_max.vx * (MAP_TILE_SIZE / 2);
                origin.vz += map_max.vz * (MAP_TILE_SIZE / 2);
                break;
            }

            EFFECT_READ_CURVE(index, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_SPAWN_SPREAD_SHIFT) & 0xf, frame);
            battle_effect_interpolate_emitter_spawn_spread(entry, factor, &spread);
            if (orient) {
                SetRotMatrix(&facing);
                rotation.vx = spread.vx;
                rotation.vy = spread.vy;
                rotation.vz = spread.vz;
                RotTrans(&rotation, &spread, &out.pad);
            }

            index = (spawn_bindings >> EFFECT_SPAWN_CURVE_RADIAL_SPEED_SHIFT) & 0xf;
            index--;
            if (index != -1) {
                factor = g_battle_effect_animation_curve_data->curves[index][frame];
                speed_min = battle_lerp_q8(
                    entry->tentacle_mesh.radial_speed_start.min, entry->tentacle_mesh.radial_speed_end.min, factor);
                speed_max = battle_lerp_q8(
                    entry->tentacle_mesh.radial_speed_start.max, entry->tentacle_mesh.radial_speed_end.max, factor);
            } else {
                speed_min = entry->tentacle_mesh.radial_speed_start.min;
                speed_max = entry->tentacle_mesh.radial_speed_start.max;
            }

            EFFECT_READ_CURVE(
                index, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_VELOCITY_DIRECTION_SPREAD_SHIFT) & 0xf, frame);
            battle_effect_interpolate_emitter_velocity_direction_spread(entry, factor, &direction_spread);
            EFFECT_READ_CURVE(
                index, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_VELOCITY_BASE_ANGLE_SHIFT) & 0xf, frame);
            battle_effect_interpolate_emitter_velocity_base_angles(entry, factor, &base_angles);

            EFFECT_READ_CURVE(index, factor, (tip_bindings >> EFFECT_TENTACLE_CURVE_TIP_OFFSET_SHIFT) & 0xf, frame);
            tip.vx = battle_lerp_q8(
                entry->tentacle_mesh.target_offset_start.x, entry->tentacle_mesh.target_offset_end.x, factor);
            tip.vy = battle_lerp_q8(
                entry->tentacle_mesh.target_offset_start.y, entry->tentacle_mesh.target_offset_end.y, factor);
            tip.vz = battle_lerp_q8(
                entry->tentacle_mesh.target_offset_start.z, entry->tentacle_mesh.target_offset_end.z, factor);
            switch (placement & EFFECT_EMITTER_HOMING_MASK) {
            case EFFECT_EMITTER_HOMING_OFFSET:
            case EFFECT_EMITTER_HOMING_OFFSET_ALT:
                break;
            case EFFECT_EMITTER_HOMING_MAP_CENTER:
                battle_map_store_max_coordinates(&tip_map_max);
                tip.vx += tip_map_max.vx * (MAP_TILE_SIZE / 2);
                tip.vz += tip_map_max.vz * (MAP_TILE_SIZE / 2);
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
                tile = battle_map_get_tile_data_pointer(g_battle_effect_target_tile.data.tile.vx,
                    g_battle_effect_target_tile.data.tile.vz, g_battle_effect_target_tile.data.tile.vy);
                tip.vx += g_battle_effect_target_tile.data.tile.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                tip.vz += g_battle_effect_target_tile.data.tile.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                tip.vy -= tile->height * MAP_HEIGHT_STEP;
                break;
            }

            EFFECT_READ_CURVE(index, factor, (tip_bindings >> EFFECT_TENTACLE_CURVE_TIP_SPREAD_SHIFT) & 0xf, frame);
            tip_spread.vx = battle_lerp_q8(
                entry->tentacle_mesh.tip_spread_start_x, entry->tentacle_mesh.tip_spread_end_x, factor);
            tip_spread.vy = battle_lerp_q8(
                entry->tentacle_mesh.tip_spread_start_y, entry->tentacle_mesh.tip_spread_end_y, factor);
            tip_spread.vz = battle_lerp_q8(
                entry->tentacle_mesh.tip_spread_start_z, entry->tentacle_mesh.tip_spread_end_z, factor);

            EFFECT_READ_CURVE(
                index, factor, (tip_bindings >> EFFECT_TENTACLE_CURVE_TRAVEL_DURATION_SHIFT) & 0xf, frame);
            length_min = battle_lerp_q8(
                entry->tentacle_mesh.lifetime_start.min, entry->tentacle_mesh.lifetime_end.min, factor);
            length_max = battle_lerp_q8(
                entry->tentacle_mesh.lifetime_start.max, entry->tentacle_mesh.lifetime_end.max, factor);

            /* Start up to count particles while free segments remain. */
            for (i = 0; i < count; i++) {
                if (work->count >= 4) {
                    break;
                }
                work->count++;
                for (j = 0; j < 4; j++) {
                    if (work->segments[j].active == 0) {
                        seg = &work->segments[j];
                        break;
                    }
                }

                /* A random direction scaled by the spread on each axis: a point on the spread ellipsoid. */
                len = battle_effect_select_largest_magnitude_s16(spread.vx, spread.vy, spread.vz);
                if (len != 0) {
                    rotation.vx = rand();
                    rotation.vy = rand();
                    rotation.vz = rand();
                    RotMatrix(&rotation, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    rotation.vx = len;
                    rotation.vz = 0;
                    rotation.vy = 0;
                    gte_RotTrans(&rotation, &out, &out.pad);
                    seg->position.vx = ((out.vx * spread.vx) / len + origin.vx) * ONE;
                    seg->position.vy = ((out.vy * spread.vy) / len + origin.vy) * ONE;
                    seg->position.vz = ((out.vz * spread.vz) / len + origin.vz) * ONE;
                } else {
                    seg->position.vx = origin.vx * ONE;
                    seg->position.vy = origin.vy * ONE;
                    seg->position.vz = origin.vz * ONE;
                }

                /* The tip gets the same treatment with its own spread. */
                len = battle_effect_select_largest_magnitude_s16(tip_spread.vx, tip_spread.vy, tip_spread.vz);
                if (len != 0) {
                    rotation.vx = rand();
                    rotation.vy = rand();
                    rotation.vz = rand();
                    RotMatrix(&rotation, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    rotation.vx = len;
                    rotation.vz = 0;
                    rotation.vy = 0;
                    RotTrans(&rotation, &out, &out.pad);
                    seg->tip.vx = tip.vx + (tip_spread.vx * out.vx) / len;
                    seg->tip.vy = tip.vy + (tip_spread.vy * out.vy) / len;
                    seg->tip.vz = tip.vz + (tip_spread.vz * out.vz) / len;
                } else {
                    seg->tip = tip;
                }

                /* Speeds are 1/512 unit a frame: << 3, or >> 9 of a 4.12 unit vector, gives 20.12. */
                switch (flags & EFFECT_EMITTER_VELOCITY_MASK) {
                case EFFECT_EMITTER_VELOCITY_OUTWARD:
                    rotation.vx = base_angles.vx + RANDOM_BETWEEN(direction_spread.vx, -direction_spread.vx);
                    rotation.vy = base_angles.vy + RANDOM_BETWEEN(direction_spread.vy, -direction_spread.vy);
                    rotation.vz = base_angles.vz + RANDOM_BETWEEN(direction_spread.vz, -direction_spread.vz);
                    RotMatrix(&rotation, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    rotation.vz = 0;
                    rotation.vx = 0;
                    rotation.vy = RANDOM_BETWEEN(speed_min, speed_max);
                    RotTrans(&rotation, &out, &out.pad);
                    seg->velocity.vx = out.vx << 3;
                    seg->velocity.vy = out.vy << 3;
                    seg->velocity.vz = out.vz << 3;
                    break;
                case EFFECT_EMITTER_VELOCITY_CASTER_FACING:
                    rotation.vx = base_angles.vx + RANDOM_BETWEEN(direction_spread.vx, -direction_spread.vx);
                    rotation.vy = base_angles.vy + RANDOM_BETWEEN(direction_spread.vy, -direction_spread.vy);
                    rotation.vz = base_angles.vz + RANDOM_BETWEEN(direction_spread.vz, -direction_spread.vz);
                    RotMatrix(&rotation, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    rotation.vz = 0;
                    rotation.vx = 0;
                    rotation.vy = RANDOM_BETWEEN(speed_min, speed_max);
                    RotTrans(&rotation, &out, &out.pad);
                    SetRotMatrix(&facing);
                    rotation.vx = out.vx;
                    rotation.vy = out.vy;
                    rotation.vz = out.vz;
                    RotTrans(&rotation, &out, &out.pad);
                    seg->velocity.vx = out.vx << 3;
                    seg->velocity.vy = out.vy << 3;
                    seg->velocity.vz = out.vz << 3;
                    break;
                case EFFECT_EMITTER_VELOCITY_INWARD:
                    out.vx = origin.vx - (seg->position.vx >> 12);
                    out.vy = origin.vy - (seg->position.vy >> 12);
                    out.vz = origin.vz - (seg->position.vz >> 12);
                    speed = RANDOM_BETWEEN(speed_min, speed_max);
                    /* A particle spawned on the origin has no direction to it and moves along +y. */
                    if (out.vx != 0 || out.vy != 0 || out.vz != 0) {
                        VectorNormal(&out, &out);
                        seg->velocity.vx = (speed * out.vx) >> 9;
                        seg->velocity.vy = (speed * out.vy) >> 9;
                        seg->velocity.vz = (speed * out.vz) >> 9;
                    } else {
                        seg->velocity.vy = speed << 3;
                        seg->velocity.vz = 0;
                        seg->velocity.vx = 0;
                    }
                    break;
                }

                seg->length = RANDOM_BETWEEN(length_min, length_max);
                seg->head = 0;
                seg->active = 1;

                /* Only trail points 0-8 of the 17 are seeded on the spawn position (retail loop bound). */
                rotation.vx = seg->position.vx >> 12;
                rotation.vy = seg->position.vy >> 12;
                rotation.vz = seg->position.vz >> 12;
                for (index = 0; index < 9; index++) {
                    seg->points[index].x = rotation.vx;
                    seg->points[index].y = rotation.vy;
                    seg->points[index].z = rotation.vz;
                }
            }
        }

        /* Advance every active particle; nothing is drawn. */
        for (j = 0; j < 4; j++) {
            if (work->segments[j].active == 0) {
                continue;
            }
            seg = &work->segments[j];

            /* While travelling, the newest point eases from the particle's path towards the tip. After
             * arrival, point stays on the point written on the arrival frame. */
            emitter.motion.position.x = x = seg->position.vx += seg->velocity.vx;
            emitter.motion.position.y = y = seg->position.vy += seg->velocity.vy;
            emitter.motion.position.z = z = seg->position.vz += seg->velocity.vz;
            index = ++seg->head;
            point = index % 17;
            px = x >> 12;
            py = y >> 12;
            pz = z >> 12;
            if (index <= seg->length) {
                seg->points[point].x = battle_effect_interpolate_s32_cos(px, seg->tip.vx, seg->length, index);
                seg->points[point].y = battle_effect_interpolate_s32_cos(py, seg->tip.vy, seg->length, index);
                seg->points[point].z = battle_effect_interpolate_s32_cos(pz, seg->tip.vz, seg->length, index);
            } else {
                seg->points[point].x = seg->tip.vx;
                seg->points[point].y = seg->tip.vy;
                seg->points[point].z = seg->tip.vz;
                tail_step = index - 16;
                shade_index = seg->length - tail_step;
                if (shade_index < 0) {
                    shade_index = 0;
                }
                wrap = point - 16;
                point = wrap + shade_index;
                if (point < 0) {
                    point += 17;
                }
            }

            /* Children spawn from that point. */
            emitter.motion.position.x = seg->points[point].x * ONE;
            emitter.motion.position.y = seg->points[point].y * ONE;
            emitter.motion.position.z = seg->points[point].z * ONE;
            if (flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) {
                battle_effect_spawn_emitter_particles(
                    record->record_index, seg->head, entry->tentacle_mesh.child_emitter_mid_life, &emitter);
            }

            /* Done once the trail has run out: the travel time plus a frame per trailing segment. */
            if (seg->length + 16 == seg->head) {
                if (flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK) {
                    battle_effect_spawn_emitter_particles(
                        record->record_index, seg->head, entry->tentacle_mesh.child_emitter_on_death, &emitter);
                }
                seg->active = 0;
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
            battle_heap_free_block(work);
            record->work_slots[callback_slot] = 0;
        }
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
