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

/* Run one phase of a tentacle-mesh effect: allocate its segment quads, spawn
 * up to 8 segments on the emitter interval, then advance each segment and
 * draw it as a strip of paired POLY_GT4 quads. */
s32 effect_e453_update_tentacle_mesh_state(s16 record_index, s32 byte_offset, s32 geometry_index, s32 frame) {
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
    VECTOR screen0;
    VECTOR screen1;
    effect_work_record_t emitter;
    SVECTOR map_max;
    effect_record_t* record;
    effect_tentacle_mesh_work_t* work;
    effect_tentacle_mesh_geometry_view_t* entry;
    s16 speed_min;
    s16 speed_max;
    s16 length_min;
    s16 length_max;
    s32 i;
    s32 count;
    s32 j;
    u16 flags;
    s32 shade_index;
    s32 link_count;
    s32 u;
    s32 v;
    s32 width;
    s32 height;
    s32 depth_bias;
    s16 prev_angle;
    s16 angle;
    u32 red;
    u32 green;
    u32 blue;
    u32* ot;
    effect_tentacle_mesh_segment_t* seg;
    POLY_GT4* quad;
    POLY_GT4* offset_quad;
    POLY_GT4* prev;
    POLY_GT4* prev_offset_quad;
    map_tile_t* tile;
    u16 placement;
    u16 texture_page_flags;
    u16 tpage;
    s32 translucent;
    s32 orient;
    s32 target_index;
    s32 curves0;
    s32 curves1;
    s32 abe;
    s32 index;
    s32 factor;
    s16 len;
    s32 speed;
    s32 brightness_row;
    s32 scratch; /* one register: INIT column, edge half-width x, then brightness factor */
    s32 dy;
    s32 depth;
    s16 edge_angle;
    s32 point_index;
    battle_effect_target_t* caster_target;
    s32 px;
    s32 tail_step;
    s32 x;
    s32 y;
    s32 z;
    s32 mid_angle;
    s16 facing_yaw;
    s32 py;
    s32 pz;
    s32 wrap;

    record = &g_effect_state_records[record_index];
    entry = (effect_tentacle_mesh_geometry_view_t*)&g_effect_geometry_table->entries[geometry_index];
    placement = entry->placement_flags;
    flags = entry->flags;
    ot = main_gfx_get_otag();

    switch (record->phase[byte_offset]) {
    case EFFECT_PHASE_IDLE:
        break;

    case EFFECT_PHASE_INIT:
        if (record->work_slots[byte_offset] != 0) {
            battle_heap_free_block(record->work_slots[byte_offset]);
        }
        work = record->work_slots[byte_offset]
            = battle_heap_alloc_block(sizeof(effect_tentacle_mesh_work_t), record_index);
        texture_page_flags = entry->texture_page_flags;
        abe = texture_page_flags & 4;
        translucent = abe == 0;
        tpage = ((texture_page_flags & 3) << 5) | 0x86;
        for (i = 0; i < 8; i++) {
            for (scratch = 0; scratch < 8; scratch++) {
                quad = &work->segments[i].quads[0][scratch];
                SetPolyGT4(quad);
                if (translucent) {
                    quad->code |= 2;
                } else {
                    quad->code &= ~2;
                }
                quad->tpage = tpage;
                quad->clut = 0x7B00;
                work->segments[i].quads[1][scratch] = *quad;
                work->segments[i].quads[2][scratch] = *quad;
                work->segments[i].quads[3][scratch] = *quad;
            }
            work->segments[i].active = 0;
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
                if (work->count >= 8) {
                    break;
                }
                work->count++;
                for (j = 0; j < 8; j++) {
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
                seg->frame = 0;
                seg->frame_timer = entry->frame_delay;
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

        width = entry->texture_width;
        height = entry->texture_height;
        depth_bias = entry->ot_depth;
        brightness_row = entry->brightness_row;
        for (j = 0; j < 8; j++) {
            if (work->segments[j].active == 0) {
                continue;
            }
            seg = &work->segments[j];
            if (--seg->frame_timer == 0) {
                if (++seg->frame > entry->frame_count - 1) {
                    seg->frame = 0;
                }
                seg->frame_timer = entry->frame_delay;
            }
            if (entry->flags & EFFECT_GEOMETRY_FLAG_COLOUR_CURVES) {
                red = g_effect_palette_table[entry->colour_curves & 0xF].factor[seg->head];
                factor = entry->colour_curves & 0xFFF;
                green = g_effect_palette_table[(factor >> 4) & 0xF].factor[seg->head];
                blue = g_effect_palette_table[(u32)factor >> 8].factor[seg->head];
            } else {
                red = green = blue = 0x80;
            }

            emitter.position[0] = x = seg->position.vx += seg->velocity.vx;
            emitter.position[1] = y = seg->position.vy += seg->velocity.vy;
            emitter.position[2] = z = seg->position.vz += seg->velocity.vz;
            index = ++seg->head;
            point_index = index % 9;
            px = x >> 12;
            py = y >> 12;
            pz = z >> 12;
            if (index <= seg->length) {
                seg->points[point_index].x = battle_effect_interpolate_s32_cos(px, seg->tip.vx, seg->length, index);
                seg->points[point_index].y = battle_effect_interpolate_s32_cos(py, seg->tip.vy, seg->length, index);
                seg->points[point_index].z = battle_effect_interpolate_s32_cos(pz, seg->tip.vz, seg->length, index);
                /* Assigned after the calls; the scheduler hoists both stores above them. */
                shade_index = 8;
                link_count = 8;
                if (index < 8) {
                    link_count = index;
                }
                u = entry->texture_u;
            } else {
                seg->points[point_index].x = seg->tip.vx;
                seg->points[point_index].y = seg->tip.vy;
                seg->points[point_index].z = seg->tip.vz;
                tail_step = index - 8;
                shade_index = seg->length - tail_step;
                if (shade_index < 0) {
                    shade_index = 0;
                }
                link_count = shade_index;
                wrap = point_index - 8;
                point_index = wrap + shade_index;
                if (point_index < 0) {
                    point_index += 9;
                }
                u = entry->texture_u + (8 - shade_index) * (width / 8);
            }

            emitter.position[0] = seg->points[point_index].x << 12;
            emitter.position[1] = seg->points[point_index].y << 12;
            emitter.position[2] = seg->points[point_index].z << 12;
            if (flags & 0xC) {
                battle_effect_spawn_emitter_particles(
                    record->record_index, seg->head, entry->child_emitter_mid_life, &emitter);
            }
            v = entry->texture_v + seg->frame * height;
            quad = &seg->quads[work->buffer][0];
            offset_quad = &seg->quads[work->buffer + 2][0];
            SetRotMatrix(&g_battle_camera_matrix);
            SetTransMatrix(&g_battle_camera_matrix);
            rotation.vx = seg->points[point_index].x;
            rotation.vy = seg->points[point_index].y;
            rotation.vz = seg->points[point_index].z;
            RotTrans(&rotation, &screen0, &screen0.pad);
            point_index--;
            if (point_index < 0) {
                point_index = 8;
            }
            rotation.vx = seg->points[point_index].x;
            rotation.vy = seg->points[point_index].y;
            rotation.vz = seg->points[point_index].z;
            RotTrans(&rotation, &screen1, &screen1.pad);
            prev_angle = ratan2(screen1.vy - screen0.vy, screen1.vx - screen0.vx);
            scratch = ((rcos(prev_angle + 0x400) * height) / 2) >> 12;
            dy = ((rsin(prev_angle + 0x400) * height) / 2) >> 12;
            depth = (screen0.vz + screen1.vz) / 8 + depth_bias;
            quad->x0 = screen0.vx - scratch;
            offset_quad->x0 = quad->x0 + 1;
            quad->y0 = screen0.vy - dy;
            offset_quad->y0 = quad->y0 + 1;
            quad->x2 = screen0.vx + scratch;
            offset_quad->x2 = quad->x2 + 1;
            quad->y2 = screen0.vy + dy;
            offset_quad->y2 = quad->y2 + 1;
            offset_quad->u0 = offset_quad->u2 = quad->u0 = quad->u2 = u;
            offset_quad->v0 = quad->v0 = v;
            offset_quad->v2 = quad->v2 = v + height;
            scratch = g_effect_e453_tentacle_mesh_brightness_table[brightness_row][shade_index];
            shade_index--;
            offset_quad->r0 = offset_quad->r2 = quad->r0 = quad->r2 = (scratch * red) >> 12;
            offset_quad->g0 = offset_quad->g2 = quad->g0 = quad->g2 = (scratch * green) >> 12;
            offset_quad->b0 = offset_quad->b2 = quad->b0 = quad->b2 = (scratch * blue) >> 12;

            for (index = 1; index < link_count; index++) {
                point_index--;
                prev = quad;
                prev_offset_quad = offset_quad;
                quad = &seg->quads[work->buffer][index];
                offset_quad = &seg->quads[work->buffer + 2][index];
                screen0 = screen1;
                if (point_index < 0) {
                    point_index = 8;
                }
                rotation.vx = seg->points[point_index].x;
                rotation.vy = seg->points[point_index].y;
                rotation.vz = seg->points[point_index].z;
                RotTrans(&rotation, &screen1, &screen1.pad);
                angle = ratan2(screen1.vy - screen0.vy, screen1.vx - screen0.vx);
                mid_angle = (angle + prev_angle) / 2;
                edge_angle = mid_angle + 0x400;
                if (angle - prev_angle < -0x800 || angle - prev_angle > 0x800) {
                    edge_angle = mid_angle + 0xC00;
                }
                scratch = ((rcos(edge_angle) * height) / 2) >> 12;
                dy = ((rsin(edge_angle) * height) / 2) >> 12;
                prev_offset_quad->x1 = offset_quad->x0 = (prev->x1 = quad->x0 = screen0.vx - scratch) + 1;
                prev_offset_quad->y1 = offset_quad->y0 = (prev->y1 = quad->y0 = screen0.vy - dy) + 1;
                prev_offset_quad->x3 = offset_quad->x2 = (prev->x3 = quad->x2 = screen0.vx + scratch) + 1;
                prev_offset_quad->y3 = offset_quad->y2 = (prev->y3 = quad->y2 = screen0.vy + dy) + 1;
                prev_offset_quad->u1 = prev_offset_quad->u3 = offset_quad->u0 = offset_quad->u2 = prev->u1 = prev->u3
                    = quad->u0 = quad->u2 = u + index * (width / 8);
                prev_offset_quad->v1 = offset_quad->v0 = prev->v1 = quad->v0 = v;
                prev_offset_quad->v3 = offset_quad->v2 = prev->v3 = quad->v2 = v + height;
                scratch = g_effect_e453_tentacle_mesh_brightness_table[brightness_row][shade_index];
                shade_index--;
                prev_offset_quad->r1 = prev_offset_quad->r3 = offset_quad->r0 = offset_quad->r2 = prev->r1 = prev->r3
                    = quad->r0 = quad->r2 = (scratch * red) >> 12;
                prev_offset_quad->g1 = prev_offset_quad->g3 = offset_quad->g0 = offset_quad->g2 = prev->g1 = prev->g3
                    = quad->g0 = quad->g2 = (scratch * green) >> 12;
                prev_offset_quad->b1 = prev_offset_quad->b3 = offset_quad->b0 = offset_quad->b2 = prev->b1 = prev->b3
                    = quad->b0 = quad->b2 = (scratch * blue) >> 12;
                if (depth < EFFECT_OT_DEPTH_MIN) {
                    depth = EFFECT_OT_DEPTH_MIN;
                }
                if (depth >= EFFECT_OT_DEPTH_LIMIT) {
                    depth = EFFECT_OT_DEPTH_MAX;
                }
                AddPrim(&ot[depth], prev);
                AddPrim(&ot[depth], prev_offset_quad);
                depth = (screen0.vz + screen1.vz) / 8 + depth_bias;
                prev_angle = angle;
            }

            scratch = ((rcos(prev_angle + 0x400) * height) / 2) >> 12;
            dy = ((rsin(prev_angle + 0x400) * height) / 2) >> 12;
            quad->x1 = screen1.vx - scratch;
            offset_quad->x1 = quad->x1 + 1;
            quad->y1 = screen1.vy - dy;
            offset_quad->y1 = quad->y1 + 1;
            quad->x3 = screen1.vx + scratch;
            offset_quad->x3 = quad->x3 + 1;
            quad->y3 = screen1.vy + dy;
            offset_quad->y3 = quad->y3 + 1;
            offset_quad->u1 = offset_quad->u3 = quad->u1 = quad->u3 = u + (width / 8) * link_count;
            offset_quad->v1 = quad->v1 = v;
            offset_quad->v3 = quad->v3 = v + height;
            scratch = g_effect_e453_tentacle_mesh_brightness_table[brightness_row][shade_index];
            offset_quad->r1 = offset_quad->r3 = quad->r1 = quad->r3 = (scratch * red) >> 12;
            offset_quad->g1 = offset_quad->g3 = quad->g1 = quad->g3 = (scratch * green) >> 12;
            offset_quad->b1 = offset_quad->b3 = quad->b1 = quad->b3 = (scratch * blue) >> 12;
            if (depth < EFFECT_OT_DEPTH_MIN) {
                depth = EFFECT_OT_DEPTH_MIN;
            }
            if (depth >= EFFECT_OT_DEPTH_LIMIT) {
                depth = EFFECT_OT_DEPTH_MAX;
            }
            AddPrim(&ot[depth], offset_quad);
            AddPrim(&ot[depth], quad);

            if (seg->length + 8 == seg->head) {
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
