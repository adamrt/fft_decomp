/* Tentacle meshes: up to 8 segments spawned on the emitter interval, each drawn as a strip of shaded textured quads. */

#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/effect.h"
#include "fft/effect_file.h"
#include "fft/effect_geometry.h"
#include "fft/effect_heap.h"
#include "fft/effect_particle.h"
#include "fft/effect_script.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/unit_runtime.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Callback handler for tentacle meshes. Every spawn interval up to eight
 * tentacles start from random points of the emitter's spawn volume, each with
 * a launch velocity and a tip near the anchor that placement_flags selects.
 * Each frame a tentacle's newest trail point eases from its moving particle
 * towards the tip over its travel time (`length` frames); once there, the
 * strip shortens from its tail a point a frame. The last nine points are
 * projected and drawn as a strip of up to eight quads mitered at the joints,
 * each paired with a copy one pixel right and down. The emitter's mid-life
 * child spawns at the strip's head every frame and its death child when the
 * strip is gone. */
s32 effect_tentacle_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame) {
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
    VECTOR screen0;
    VECTOR screen1;
    effect_work_record_t emitter;
    SVECTOR map_max;
    effect_record_t* record;
    effect_tentacle_mesh_work_t* work;
    const effect_geometry_record_t* entry;
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
    u32 shade_r;
    u32 shade_g;
    u32 shade_b;
    u32* ot;
    effect_tentacle_mesh_segment_t* seg;
    POLY_GT4* quad;
    POLY_GT4* offset_quad;
    POLY_GT4* prev;
    POLY_GT4* prev_offset_quad;
    battle_map_tile_t* tile;
    u16 placement;
    effect_texture_page_config_t texture_page_config;
    u16 tpage;
    s32 translucent;
    s32 orient;
    s32 target_index;
    s32 spawn_bindings;
    s32 tip_bindings;
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
    effect_target_t* caster_target;
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

    record = &g_effect_state_records[state_index];
    entry = &g_effect_geometry_table->entries[emitter_index];
    placement = entry->tentacle_mesh.placement_flags;
    flags = entry->tentacle_mesh.flags;
    ot = gfx_get_battle_otag();

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;

    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            effect_heap_free_block(record->work_slots[callback_slot]);
        }
        work = record->work_slots[callback_slot]
            = effect_heap_alloc_block(sizeof(effect_tentacle_mesh_work_t), state_index);

        texture_page_config = entry->tentacle_mesh.texture_page_config;
        translucent = 1;
        if (texture_page_config.clear_abe) {
            translucent = 0;
        }
        tpage = (texture_page_config.blend_mode << EFFECT_RENDER_TEXTURE_PAGE_BLEND_SHIFT)
            | EFFECT_RENDER_TEXTURE_PAGE_BASE;

        /* Quad sets 0-1 are the double-buffered strips, sets 2-3 their one-pixel offset copies. */
        for (i = 0; i < 8; i++) {
            for (scratch = 0; scratch < 8; scratch++) {
                quad = &work->segments[i].quads[0][scratch];
                SetPolyGT4(quad);
                setSemiTrans(quad, translucent);
                quad->tpage = tpage;
                quad->clut = BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_EFFECT, 0);
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
                facing_yaw = ((unit_facing_get_quadrant_by_runtime_id(caster_target->data.unit.runtime_id) + 1) & 3)
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

            EFFECT_READ_CURVE(index, factor, (tip_bindings >> EFFECT_EMITTER_CURVE_PARTICLE_COUNT_SHIFT) & 0xf, frame);
            count = effect_lerp_q8(
                entry->tentacle_mesh.particle_count_start, entry->tentacle_mesh.particle_count_end, factor);

            EFFECT_READ_CURVE(index, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_SPAWN_POSITION_SHIFT) & 0xf, frame);
            effect_interpolate_emitter_spawn_position(entry, factor, &origin);
            switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
            case EFFECT_EMITTER_ORIGIN_WORLD:
            case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
                break;
            case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
                tile = battle_map_tile_get_pointer(g_effect_target_tile.data.tile.vx, g_effect_target_tile.data.tile.vz,
                    g_effect_target_tile.data.tile.vy);
                origin.vx += g_effect_target_tile.data.tile.vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
                origin.vz += g_effect_target_tile.data.tile.vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
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
                battle_map_store_max_coordinates(&map_max);
                origin.vx += map_max.vx * (BATTLE_MAP_TILE_SIZE / 2);
                origin.vz += map_max.vz * (BATTLE_MAP_TILE_SIZE / 2);
                break;
            }

            EFFECT_READ_CURVE(index, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_SPAWN_SPREAD_SHIFT) & 0xf, frame);
            effect_interpolate_emitter_spawn_spread(entry, factor, &spread);
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
            effect_interpolate_emitter_velocity_direction_spread(entry, factor, &direction_spread);
            EFFECT_READ_CURVE(
                index, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_VELOCITY_BASE_ANGLE_SHIFT) & 0xf, frame);
            effect_interpolate_emitter_velocity_base_angles(entry, factor, &base_angles);

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
                battle_map_store_max_coordinates(&tip_map_max);
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
                tile = battle_map_tile_get_pointer(g_effect_target_tile.data.tile.vx, g_effect_target_tile.data.tile.vz,
                    g_effect_target_tile.data.tile.vy);
                tip.vx += g_effect_target_tile.data.tile.vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
                tip.vz += g_effect_target_tile.data.tile.vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
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
            length_min = effect_lerp_q8(
                entry->tentacle_mesh.lifetime_start.min, entry->tentacle_mesh.lifetime_end.min, factor);
            length_max = effect_lerp_q8(
                entry->tentacle_mesh.lifetime_start.max, entry->tentacle_mesh.lifetime_end.max, factor);

            /* Start up to count tentacles while free segments remain. */
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

                /* A random direction scaled by the spread on each axis: a point on the spread ellipsoid. */
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
                seg->frame = 0;
                seg->frame_timer = entry->tentacle_mesh.frame_delay;

                /* Every trail point starts on the spawn position. */
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

        /* Advance and draw every active tentacle. */
        width = entry->tentacle_mesh.texture_width;
        height = entry->tentacle_mesh.texture_height;
        depth_bias = entry->tentacle_mesh.ot_depth;
        brightness_row = entry->tentacle_mesh.brightness_row;
        for (j = 0; j < 8; j++) {
            if (work->segments[j].active == 0) {
                continue;
            }
            seg = &work->segments[j];
            if (--seg->frame_timer == 0) {
                if (++seg->frame > entry->tentacle_mesh.frame_count - 1) {
                    seg->frame = 0;
                }
                seg->frame_timer = entry->tentacle_mesh.frame_delay;
            }

            /* The color curves are read at the tentacle's age, not the effect frame. */
            if (entry->tentacle_mesh.flags & EFFECT_EMITTER_FLAG_COLOR_CURVES) {
                factor = (entry->tentacle_mesh.color_curves & 0xfff);
                red = g_effect_animation_curve_data->curves[(factor >> EFFECT_COLOR_CURVE_RED_SHIFT) & 0xf][seg->head];
                green = g_effect_animation_curve_data
                            ->curves[(factor >> EFFECT_COLOR_CURVE_GREEN_SHIFT) & 0xf][seg->head];
                blue
                    = g_effect_animation_curve_data->curves[(factor >> EFFECT_COLOR_CURVE_BLUE_SHIFT) & 0xf][seg->head];
            } else {
                red = green = blue = EFFECT_COLOR_NEUTRAL;
            }

            /* While travelling, the newest point eases from the particle's path towards the tip. After
             * arrival the strip's head stays on the arrival point and its tail catches up a point a frame. */
            emitter.motion.position.x = x = seg->position.vx += seg->velocity.vx;
            emitter.motion.position.y = y = seg->position.vy += seg->velocity.vy;
            emitter.motion.position.z = z = seg->position.vz += seg->velocity.vz;
            index = ++seg->head;
            point_index = index % 9;
            px = x >> 12;
            py = y >> 12;
            pz = z >> 12;
            if (index <= seg->length) {
                seg->points[point_index].x = effect_interpolate_s32_cos(px, seg->tip.vx, seg->length, index);
                seg->points[point_index].y = effect_interpolate_s32_cos(py, seg->tip.vy, seg->length, index);
                seg->points[point_index].z = effect_interpolate_s32_cos(pz, seg->tip.vz, seg->length, index);
                /* Assigned after the calls; the scheduler hoists both stores above them. */
                shade_index = 8;
                link_count = 8;
                if (index < 8) {
                    link_count = index;
                }
                u = entry->tentacle_mesh.texture_u;
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
                /* Drop texture columns from the head end as the strip shortens. */
                u = entry->tentacle_mesh.texture_u + (8 - shade_index) * (width / 8);
            }

            /* Children spawn from the strip's head point. */
            emitter.motion.position.x = seg->points[point_index].x * ONE;
            emitter.motion.position.y = seg->points[point_index].y * ONE;
            emitter.motion.position.z = seg->points[point_index].z * ONE;
            if (flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) {
                effect_particle_spawn_from_emitter(
                    record->record_index, seg->head, entry->tentacle_mesh.child_emitter_mid_life, &emitter);
            }

            /* Head: the first quad's leading edge, perpendicular to the first segment (angle + 0x400).
             * Each quad has a copy in set buffer + 2, one pixel right and down. */
            v = entry->tentacle_mesh.texture_v + seg->frame * height;
            quad = &seg->quads[work->buffer][0];
            offset_quad = &seg->quads[work->buffer + 2][0];
            SetRotMatrix(&g_camera_matrix);
            SetTransMatrix(&g_camera_matrix);
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
            scratch = g_effect_tentacle_mesh_brightness_table[brightness_row][shade_index];
            shade_index--;
            setRGB0(offset_quad, offset_quad->r2 = quad->r0 = quad->r2 = (scratch * red) / ONE,
                offset_quad->g2 = quad->g0 = quad->g2 = (scratch * green) / ONE,
                offset_quad->b2 = quad->b0 = quad->b2 = (scratch * blue) / ONE);

            /* Each joint is mitered along the bisector of its two segments. Averaging angles more than
             * half a turn apart points the wrong way, so 0xc00 replaces 0x400 there. */
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
                scratch = g_effect_tentacle_mesh_brightness_table[brightness_row][shade_index];
                shade_index--;
                shade_r = (scratch * red) / ONE;
                prev_offset_quad->r1 = prev_offset_quad->r3 = offset_quad->r0 = offset_quad->r2 = prev->r1 = prev->r3
                    = quad->r0 = quad->r2 = shade_r;
                shade_g = (scratch * green) / ONE;
                prev_offset_quad->g1 = prev_offset_quad->g3 = offset_quad->g0 = offset_quad->g2 = prev->g1 = prev->g3
                    = quad->g0 = quad->g2 = shade_g;
                shade_b = (scratch * blue) / ONE;
                prev_offset_quad->b1 = prev_offset_quad->b3 = offset_quad->b0 = offset_quad->b2 = prev->b1 = prev->b3
                    = quad->b0 = quad->b2 = shade_b;

                /* The previous quad is now complete: link it at its segment's depth. */
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

            /* Tail: the last quad's trailing edge, perpendicular to the last segment. */
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
            scratch = g_effect_tentacle_mesh_brightness_table[brightness_row][shade_index];
            setRGB1(offset_quad, offset_quad->r3 = quad->r1 = quad->r3 = (scratch * red) / ONE,
                offset_quad->g3 = quad->g1 = quad->g3 = (scratch * green) / ONE,
                offset_quad->b3 = quad->b1 = quad->b3 = (scratch * blue) / ONE);

            if (depth < EFFECT_OT_DEPTH_MIN) {
                depth = EFFECT_OT_DEPTH_MIN;
            }
            if (depth >= EFFECT_OT_DEPTH_LIMIT) {
                depth = EFFECT_OT_DEPTH_MAX;
            }
            AddPrim(&ot[depth], offset_quad);
            AddPrim(&ot[depth], quad);

            /* Done once the tail reaches the tip: the travel time plus a frame per trailing segment. */
            if (seg->length + 8 == seg->head) {
                if (flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK) {
                    effect_particle_spawn_from_emitter(
                        record->record_index, seg->head, entry->tentacle_mesh.child_emitter_on_death, &emitter);
                }
                seg->active = 0;
                work->count--;
            }
        }

        /* The next update fills the other buffer while this one is drawn. */
        work->buffer = 1 - work->buffer;
        break;

    case EFFECT_CALLBACK_PHASE_DESTROY:
        work = record->work_slots[callback_slot];
        /* Free one call late, as the last update's quads may still be drawing. The null
         * check below comes after this read through work (retail order). */
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
