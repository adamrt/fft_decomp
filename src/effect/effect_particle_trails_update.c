#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/effect.h"
#include "fft/map.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Runs up to four unseen particles from the emitter's spawn volume towards a
 * tip, keeping a 17-point trail of each, and spawns the emitter's children
 * from the trail's far end: the mid-life child every frame and the death
 * child when the trail has run out. The tentacle mesh renderer without its
 * mesh. */
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
    effect_record_t* record;
    effect_trail_work_t* work;
    const effect_geometry_record_t* entry;
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
    u16 placement;
    u16 flags;
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

    record = &g_effect_state_records[state_index];
    entry = &g_effect_geometry_table->entries[emitter_index];
    placement = entry->tentacle_mesh.placement_flags;
    flags = entry->tentacle_mesh.flags;

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
        i = frame - 1;
        work = record->work_slots[callback_slot];
        tip_bindings = entry->tentacle_mesh.tip_curve_bindings;
        target_index = record->timeline.target_index;
        spawn_bindings = entry->tentacle_mesh.spawn_curve_bindings;
        frame %= EFFECT_ANIMATION_CURVE_FRAMES;
        EFFECT_READ_CURVE(index, factor,
            effect_get_cached_curve_binding(tip_bindings, EFFECT_TENTACLE_CURVE_SPAWN_INTERVAL_SHIFT), frame);
        if (i
                % battle_lerp_fixed8(
                    entry->tentacle_mesh.spawn_interval_start, entry->tentacle_mesh.spawn_interval_end, factor)
            == 0) {
            SetTransMatrix(&work->matrix);
            orient = (flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING;
            caster_target = &g_effect_context.targets[EFFECT_CASTER_ENTRY];
            if (caster_target->target_type == EFFECT_TARGET_TYPE_UNIT) {
                facing_yaw
                    = ((battle_unit_get_facing_quadrant_by_runtime_id(caster_target->data.unit.runtime_id) + 1) & 3)
                    << 10;
                rotation.vz = 0;
                rotation.vx = 0;
                rotation.vy = facing_yaw;
                RotMatrix(&rotation, &facing);
            }
            effect_resolve_target_world_coords(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
                EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &caster);
            effect_resolve_target_world_coords(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
                target_index, &g_effect_context, &target);

            EFFECT_READ_CURVE(index, factor,
                effect_get_cached_curve_binding(tip_bindings, EFFECT_TENTACLE_CURVE_PARTICLE_COUNT_SHIFT), frame);
            count = battle_lerp_fixed8(
                entry->tentacle_mesh.particle_count_start, entry->tentacle_mesh.particle_count_end, factor);
            EFFECT_READ_CURVE(index, factor,
                effect_get_cached_curve_binding(spawn_bindings, EFFECT_TENTACLE_CURVE_SPAWN_POSITION_SHIFT), frame);
            effect_interpolate_emitter_spawn_position(entry, factor, &origin);
            switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
            case EFFECT_EMITTER_ORIGIN_WORLD:
            case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
                break;
            case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
                tile = map_get_tile_data_pointer(g_effect_target_tile.data.tile.vx, g_effect_target_tile.data.tile.vz,
                    g_effect_target_tile.data.tile.vy);
                origin.vx += g_effect_target_tile.data.tile.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                origin.vz += g_effect_target_tile.data.tile.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
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
                map_store_max_coordinates(&map_max);
                origin.vx += map_max.vx * (MAP_TILE_SIZE / 2);
                origin.vz += map_max.vz * (MAP_TILE_SIZE / 2);
                break;
            }

            EFFECT_READ_CURVE(index, factor,
                effect_get_cached_curve_binding(spawn_bindings, EFFECT_TENTACLE_CURVE_SPAWN_SPREAD_SHIFT), frame);
            effect_interpolate_emitter_spawn_spread(entry, factor, &spread);
            if (orient) {
                SetRotMatrix(&facing);
                rotation.vx = spread.vx;
                rotation.vy = spread.vy;
                rotation.vz = spread.vz;
                RotTrans(&rotation, &spread, &out.pad);
            }

            index = effect_get_cached_curve_binding(spawn_bindings, EFFECT_TENTACLE_CURVE_RADIAL_SPEED_SHIFT);
            index--;
            if (index != -1) {
                factor = g_effect_animation_curve_data->curves[index][frame];
                speed_min = battle_lerp_fixed8(
                    entry->tentacle_mesh.radial_speed_start.min, entry->tentacle_mesh.radial_speed_end.min, factor);
                speed_max = battle_lerp_fixed8(
                    entry->tentacle_mesh.radial_speed_start.max, entry->tentacle_mesh.radial_speed_end.max, factor);
            } else {
                speed_min = entry->tentacle_mesh.radial_speed_start.min;
                speed_max = entry->tentacle_mesh.radial_speed_start.max;
            }

            EFFECT_READ_CURVE(index, factor,
                effect_get_cached_curve_binding(spawn_bindings, EFFECT_TENTACLE_CURVE_VELOCITY_DIRECTION_SPREAD_SHIFT),
                frame);
            effect_interpolate_emitter_velocity_direction_spread(entry, factor, &direction_spread);
            EFFECT_READ_CURVE(index, factor,
                effect_get_cached_curve_binding(spawn_bindings, EFFECT_TENTACLE_CURVE_VELOCITY_BASE_ANGLE_SHIFT),
                frame);
            effect_interpolate_emitter_velocity_base_angles(entry, factor, &base_angles);

            EFFECT_READ_CURVE(index, factor,
                effect_get_cached_curve_binding(tip_bindings, EFFECT_TENTACLE_CURVE_TIP_OFFSET_SHIFT), frame);
            tip.vx = battle_lerp_fixed8(
                entry->tentacle_mesh.target_offset_start.x, entry->tentacle_mesh.target_offset_end.x, factor);
            tip.vy = battle_lerp_fixed8(
                entry->tentacle_mesh.target_offset_start.y, entry->tentacle_mesh.target_offset_end.y, factor);
            tip.vz = battle_lerp_fixed8(
                entry->tentacle_mesh.target_offset_start.z, entry->tentacle_mesh.target_offset_end.z, factor);
            switch (placement & EFFECT_EMITTER_HOMING_MASK) {
            case EFFECT_EMITTER_HOMING_OFFSET:
            case EFFECT_EMITTER_HOMING_OFFSET_ALT:
                break;
            case EFFECT_EMITTER_HOMING_MAP_CENTER:
                map_store_max_coordinates(&tip_map_max);
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
                tile = map_get_tile_data_pointer(g_effect_target_tile.data.tile.vx, g_effect_target_tile.data.tile.vz,
                    g_effect_target_tile.data.tile.vy);
                tip.vx += g_effect_target_tile.data.tile.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                tip.vz += g_effect_target_tile.data.tile.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                tip.vy -= tile->height * MAP_HEIGHT_STEP;
                break;
            }

            EFFECT_READ_CURVE(index, factor,
                effect_get_cached_curve_binding(tip_bindings, EFFECT_TENTACLE_CURVE_TIP_SPREAD_SHIFT), frame);
            tip_spread.vx = battle_lerp_fixed8(
                entry->tentacle_mesh.tip_spread_start_x, entry->tentacle_mesh.tip_spread_end_x, factor);
            tip_spread.vy = battle_lerp_fixed8(
                entry->tentacle_mesh.tip_spread_start_y, entry->tentacle_mesh.tip_spread_end_y, factor);
            tip_spread.vz = battle_lerp_fixed8(
                entry->tentacle_mesh.tip_spread_start_z, entry->tentacle_mesh.tip_spread_end_z, factor);

            EFFECT_READ_CURVE(index, factor,
                effect_get_cached_curve_binding(tip_bindings, EFFECT_TENTACLE_CURVE_TRAVEL_DURATION_SHIFT), frame);
            length_min = battle_lerp_fixed8(
                entry->tentacle_mesh.lifetime_start.min, entry->tentacle_mesh.lifetime_end.min, factor);
            length_max = battle_lerp_fixed8(
                entry->tentacle_mesh.lifetime_start.max, entry->tentacle_mesh.lifetime_end.max, factor);

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

                len = effect_select_largest_magnitude_s16(spread.vx, spread.vy, spread.vz);
                if (len != 0) {
                    rotation.vx = rand();
                    rotation.vy = rand();
                    rotation.vz = rand();
                    RotMatrix(&rotation, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    rotation.vx = len;
                    rotation.vz = 0;
                    rotation.vy = 0;
                    gte_ldv0(&rotation);
                    gte_rtv0tr();
                    gte_stlvnl(&out);
                    gte_stflg(&out.pad);
                    seg->position.vx = ((out.vx * spread.vx) / len + origin.vx) * ONE;
                    seg->position.vy = ((out.vy * spread.vy) / len + origin.vy) * ONE;
                    seg->position.vz = ((out.vz * spread.vz) / len + origin.vz) * ONE;
                } else {
                    seg->position.vx = origin.vx * ONE;
                    seg->position.vy = origin.vy * ONE;
                    seg->position.vz = origin.vz * ONE;
                }

                len = effect_select_largest_magnitude_s16(tip_spread.vx, tip_spread.vy, tip_spread.vz);
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

        for (j = 0; j < 4; j++) {
            if (work->segments[j].active == 0) {
                continue;
            }
            seg = &work->segments[j];
            emitter.motion.position.x = x = seg->position.vx += seg->velocity.vx;
            emitter.motion.position.y = y = seg->position.vy += seg->velocity.vy;
            emitter.motion.position.z = z = seg->position.vz += seg->velocity.vz;
            index = ++seg->head;
            point = index % 17;
            px = x >> 12;
            py = y >> 12;
            pz = z >> 12;
            if (index <= seg->length) {
                seg->points[point].x = effect_interpolate_s32_cos(px, seg->tip.vx, seg->length, index);
                seg->points[point].y = effect_interpolate_s32_cos(py, seg->tip.vy, seg->length, index);
                seg->points[point].z = effect_interpolate_s32_cos(pz, seg->tip.vz, seg->length, index);
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
            emitter.motion.position.x = seg->points[point].x * ONE;
            emitter.motion.position.y = seg->points[point].y * ONE;
            emitter.motion.position.z = seg->points[point].z * ONE;
            if (flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) {
                effect_spawn_emitter_particles(
                    record->record_index, seg->head, entry->tentacle_mesh.child_emitter_mid_life, &emitter);
            }
            if (seg->length + 16 == seg->head) {
                if (flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK) {
                    effect_spawn_emitter_particles(
                        record->record_index, seg->head, entry->tentacle_mesh.child_emitter_on_death, &emitter);
                }
                seg->active = 0;
                work->count--;
            }
        }
        work->buffer = 1 - work->buffer;
        break;

    case EFFECT_CALLBACK_PHASE_DESTROY:
        work = record->work_slots[callback_slot];
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
