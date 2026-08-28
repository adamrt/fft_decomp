#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "fft/effect_tentacle_mesh.h"
#include "fft/map.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/types.h"

s32 effect_e485_update_particle_trails_state(s16 record_index, s32 byte_offset, s32 geometry_index, s32 frame) {
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
    effect_tentacle_mesh_geometry_view_t* entry;
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
    s32 curves0;
    s32 curves1;
    s32 index;
    s32 factor;
    s16 len;
    s32 speed;
    s32 point;
    battle_effect_target_t* caster_target;
    s32 px;
    s32 tail_step;
    s32 x;
    s32 y;
    s32 z;
    s16 facing_yaw;
    s32 py;
    s32 pz;
    s32 wrap;

    record = &g_effect_state_records[record_index];
    entry = (effect_tentacle_mesh_geometry_view_t*)&g_effect_geometry_table->entries[geometry_index];
    placement = entry->placement_flags;
    flags = entry->flags;

    switch (record->phase[byte_offset]) {
    case EFFECT_PHASE_IDLE:
        break;

    case EFFECT_PHASE_INIT:
        if (record->work_slots[byte_offset] != 0) {
            battle_heap_free_block(record->work_slots[byte_offset]);
        }
        work = record->work_slots[byte_offset] = battle_heap_alloc_block(sizeof(effect_trail_work_t), record_index);
        for (index = 3; index >= 0; index--) {
            work->segments[index].active = 0;
        }
        work->buffer = 0;
        work->count = 0;
        work->destroy_delay = 0;
        work->matrix.t[2] = 0;
        work->matrix.t[1] = 0;
        work->matrix.t[0] = 0;
        record->phase[byte_offset] = EFFECT_PHASE_UPDATE;
        break;

    case EFFECT_PHASE_UPDATE:
        i = frame - 1;
        work = record->work_slots[byte_offset];
        curves1 = entry->curve_indices[1];
        target_index = ((effect_tentacle_mesh_record_view_t*)record)->target_index;
        curves0 = entry->curve_indices[0];
        frame %= 160;
        READ_CURVE(factor, index, (curves1 >> 24) & 0xF, frame);
        if (i % battle_effect_lerp_linear(entry->spawn_interval_start, entry->spawn_interval_end, factor) == 0) {
            SetTransMatrix(&work->matrix);
            orient = (flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING;
            caster_target = &g_battle_effect_coord_data.targets[16];
            if (caster_target->target_type == 0) {
                facing_yaw = ((battle_unit_get_facing_quadrant_by_misc_id(caster_target->id.misc_id) + 1) & 3) << 10;
                rotation.vz = 0;
                rotation.vx = 0;
                rotation.vy = facing_yaw;
                RotMatrix(&rotation, &facing);
            }
            battle_effect_resolve_target_render_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, -1,
                &g_battle_effect_coord_data, &caster);
            battle_effect_resolve_target_render_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, target_index,
                &g_battle_effect_coord_data, &target);

            READ_CURVE(factor, index, (curves1 >> 20) & 0xF, frame);
            count = battle_effect_lerp_linear(entry->particle_count_start, entry->particle_count_end, factor);
            READ_CURVE(factor, index, curves0 & 0xF, frame);
            battle_effect_interpolate_emitter_spawn_position(entry, factor, &origin.vx);
            switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
            case EFFECT_EMITTER_ORIGIN_WORLD:
            case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
                break;
            case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
                tile = battle_map_get_tile_data_pointer(g_battle_effect_target_tile.id.tile_x,
                    g_battle_effect_target_tile.tile_y, g_battle_effect_target_tile.map_z);
                origin.vx += g_battle_effect_target_tile.id.tile_x * 28 + 14;
                origin.vz += g_battle_effect_target_tile.tile_y * 28 + 14;
                origin.vy -= tile->height * 12;
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
            case EFFECT_EMITTER_ORIGIN_MAP_CENTRE:
                battle_map_store_max_coordinates(&map_max);
                origin.vx += map_max.vx * 14;
                origin.vz += map_max.vz * 14;
                break;
            }

            READ_CURVE(factor, index, (curves0 >> 4) & 0xF, frame);
            battle_effect_interpolate_emitter_spawn_spread(entry, factor, &spread.vx);
            if (orient) {
                SetRotMatrix(&facing);
                rotation.vx = spread.vx;
                rotation.vy = spread.vy;
                rotation.vz = spread.vz;
                RotTrans(&rotation, &spread, &out.pad);
            }

            index = (u32)curves0 >> 28;
            index--;
            if (index != -1) {
                factor = g_effect_palette_table[index].factor[frame];
                speed_min
                    = battle_effect_lerp_linear(entry->radial_speed_min_start, entry->radial_speed_min_end, factor);
                speed_max
                    = battle_effect_lerp_linear(entry->radial_speed_max_start, entry->radial_speed_max_end, factor);
            } else {
                speed_min = entry->radial_speed_min_start;
                speed_max = entry->radial_speed_max_start;
            }

            READ_CURVE(factor, index, (curves0 >> 12) & 0xF, frame);
            battle_effect_interpolate_emitter_velocity_direction_spread(entry, factor, &direction_spread.vx);
            READ_CURVE(factor, index, (curves0 >> 8) & 0xF, frame);
            battle_effect_interpolate_emitter_velocity_base_angles(entry, factor, &base_angles.vx);

            READ_CURVE(factor, index, (curves1 >> 12) & 0xF, frame);
            tip.vx = battle_effect_lerp_linear(entry->target_offset_start[0], entry->target_offset_end[0], factor);
            tip.vy = battle_effect_lerp_linear(entry->target_offset_start[1], entry->target_offset_end[1], factor);
            tip.vz = battle_effect_lerp_linear(entry->target_offset_start[2], entry->target_offset_end[2], factor);
            switch (placement & EFFECT_EMITTER_HOMING_MASK) {
            case EFFECT_EMITTER_HOMING_OFFSET:
            case EFFECT_EMITTER_HOMING_OFFSET_ALT:
                break;
            case EFFECT_EMITTER_HOMING_MAP_CENTRE:
                battle_map_store_max_coordinates(&tip_map_max);
                tip.vx += tip_map_max.vx * 14;
                tip.vz += tip_map_max.vz * 14;
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
                tile = battle_map_get_tile_data_pointer(g_battle_effect_target_tile.id.tile_x,
                    g_battle_effect_target_tile.tile_y, g_battle_effect_target_tile.map_z);
                tip.vx += g_battle_effect_target_tile.id.tile_x * 28 + 14;
                tip.vz += g_battle_effect_target_tile.tile_y * 28 + 14;
                tip.vy -= tile->height * 12;
                break;
            }

            READ_CURVE(factor, index, (curves1 >> 4) & 0xF, frame);
            tip_spread.vx = battle_effect_lerp_linear(entry->tip_spread_start_x, entry->tip_spread_end_x, factor);
            tip_spread.vy = battle_effect_lerp_linear(entry->tip_spread_start_y, entry->tip_spread_end_y, factor);
            tip_spread.vz = battle_effect_lerp_linear(entry->tip_spread_start_z, entry->tip_spread_end_z, factor);

            READ_CURVE(factor, index, (curves1 >> 8) & 0xF, frame);
            length_min = battle_effect_lerp_linear(entry->lifetime_start[0], entry->lifetime_end[0], factor);
            length_max = battle_effect_lerp_linear(entry->lifetime_start[1], entry->lifetime_end[1], factor);

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
                    gte_ldv0(&rotation);
                    gte_rtv0tr();
                    gte_stlvnl(&out);
                    gte_stflg(&out.pad);
                    seg->position.vx = ((out.vx * spread.vx) / len + origin.vx) << 12;
                    seg->position.vy = ((out.vy * spread.vy) / len + origin.vy) << 12;
                    seg->position.vz = ((out.vz * spread.vz) / len + origin.vz) << 12;
                } else {
                    seg->position.vx = origin.vx << 12;
                    seg->position.vy = origin.vy << 12;
                    seg->position.vz = origin.vz << 12;
                }

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
            emitter.position[0] = x = seg->position.vx += seg->velocity.vx;
            emitter.position[1] = y = seg->position.vy += seg->velocity.vy;
            emitter.position[2] = z = seg->position.vz += seg->velocity.vz;
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
            emitter.position[0] = seg->points[point].x << 12;
            emitter.position[1] = seg->points[point].y << 12;
            emitter.position[2] = seg->points[point].z << 12;
            if (flags & 0xC) {
                battle_effect_spawn_emitter_particles(
                    record->record_index, seg->head, entry->child_emitter_mid_life, &emitter);
            }
            if (seg->length + 16 == seg->head) {
                if (flags & 3) {
                    battle_effect_spawn_emitter_particles(
                        record->record_index, seg->head, entry->child_emitter_on_death, &emitter);
                }
                seg->active = 0;
                work->count--;
            }
        }
        work->buffer = 1 - work->buffer;
        break;

    case EFFECT_PHASE_DESTROY:
        work = record->work_slots[byte_offset];
        if (work->destroy_delay == 0) {
            work->destroy_delay = 1;
            break;
        }
        if (work != 0) {
            battle_heap_free_block(work);
            record->work_slots[byte_offset] = 0;
        }
        record->phase[byte_offset] = EFFECT_PHASE_IDLE;
        break;
    }
}
