/* Homing particles drawn as textured quad ribbons along their last 17 positions, up to 4 per spawn. */

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
#include "fft/unit_runtime.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

extern u32 g_effect_particle_ribbon_long_brightness_table[][EFFECT_PARTICLE_RIBBON_LONG_POINTS];

/* One long ribbon: the short layout with 16 quads per buffer and a 17-point
 * trail. */
typedef struct effect_particle_ribbon_long {
    SVECTOR target;                                            /* 0x000: homing destination */
    VECTOR velocity;                                           /* 0x008: 20.12 fixed */
    VECTOR position;                                           /* 0x018: 20.12 fixed */
    POLY_GT4 quads[2][EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1]; /* 0x028 */
    vec3s16_t trail[EFFECT_PARTICLE_RIBBON_LONG_POINTS];       /* 0x6a8: ring buffer indexed by age */
    s16 age;                                                   /* 0x70e */
    s16 lifetime;                                              /* 0x710 */
    s16 active;                                                /* 0x712 */
    u8 _unused_714[0x02];
    s16 frame;             /* 0x716: texture animation frame */
    s16 frame_timer;       /* 0x718 */
    u8 _padding_71a[0x02]; /* tail padding to 4-byte alignment */
} effect_particle_ribbon_long_t;
typedef char effect_particle_ribbon_long_size_must_be_1820[sizeof(effect_particle_ribbon_long_t) == 1820 ? 1 : -1];

/* Work block allocated by the long variant. */
typedef struct effect_particle_ribbon_long_work {
    s32 buffer;                                                               /* 0x0000 */
    s16 count;                                                                /* 0x0004: active ribbons */
    u8 _padding_0006[0x02];                                                   /* aligns ribbons */
    effect_particle_ribbon_long_t ribbons[EFFECT_PARTICLE_RIBBON_LONG_SLOTS]; /* 0x0008 */
    MATRIX matrix;                                                            /* 0x1c78 */
    s32 destroy_delay;                                                        /* 0x1c98 */
} effect_particle_ribbon_long_work_t;
typedef char
    effect_particle_ribbon_long_work_size_must_be_7324[sizeof(effect_particle_ribbon_long_work_t) == 7324 ? 1 : -1];

/* A random value in [-|x|, |x|). */
#define RAND_SPREAD(x) ((x) == -(x) ? (x) : (-(x) < (x) ? rand() % ((x) * 2) - (x) : rand() % (-(x) - (x)) + (x)))

/* A random value between `a` and `b`, in either order. */
#define RAND_BETWEEN(a, b) ((a) == (b) ? (a) : ((b) < (a) ? (b) + rand() % ((a) - (b)) : (a) + rand() % ((b) - (a))))

/* Spawns homing particles and draws each one's recent path as a ribbon of
 * textured quads.
 *
 * Every spawn interval up to 4 ribbons start from the emitter's
 * spawn volume with a randomized heading and speed (optionally turned to the
 * caster's facing) and ease towards a spread target over their lifetime. Each
 * update appends the particle's position to a 17-point ring buffer and
 * rebuilds the double-buffered quad strip along it, mitered at each joint.
 *
 * Matching notes: `quadrant` is assigned twice so the scheduler keeps the
 * RotMatrix arguments after the angle arithmetic; `point` and `index` are spelled
 * so fold keeps the target's (j - 16) and (index - 16) subexpressions; the
 * head point is re-read through the same `slot->trail[index]` address before
 * `index` steps back to the tail. */
s32 effect_particle_ribbon_long_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame) {
    s16 state_index = record_index;
    SVECTOR target_base;
    SVECTOR target_spread;
    VECTOR origin;
    VECTOR spread;
    VECTOR angle_base;
    VECTOR angle_spread;
    SVECTOR caster;
    SVECTOR target;
    SVECTOR vector;
    VECTOR result;
    SVECTOR map_max;
    MATRIX facing;
    VECTOR head;
    VECTOR tail;
    effect_work_record_t parent;
    SVECTOR map_origin;
    effect_record_t* record;
    effect_particle_ribbon_long_work_t* work;
    const effect_geometry_record_t* entry;
    s16 speed_min;
    s16 speed_max;
    s16 lifetime_min;
    s16 lifetime_max;
    s32 i;
    s32 count;
    s32 k;
    u16 flags;
    s32 segments;
    s32 u;
    s32 v;
    s32 width;
    s32 column_width;
    s32 height;
    s32 depth_base;
    s16 angle;
    s16 next_angle;
    u32 red;
    u32 green;
    u32 blue;
    u32* ot;
    u16 placement;
    s32 target_bindings;
    s32 spawn_bindings;
    s32 target_index;
    s32 factor;
    s32 rotate;
    s16 quadrant;
    s32 translucent;
    u16 tpage;
    effect_texture_page_config_t configuration;
    s16 largest;
    s32 speed;
    s32 row;
    s32 j;
    s32 index;
    s32 x;
    s32 y;
    s32 z;
    s32 point;
    s32 offset_x;
    s32 offset_y;
    s32 depth;
    s16 mid;
    s32 bisector;
    effect_particle_ribbon_long_t* slot;
    POLY_GT4* quad;
    POLY_GT4* prev;
    battle_map_tile_t* tile;
    effect_target_t* caster_target;

    record = &g_effect_state_records[state_index];
    entry = &g_effect_geometry_table->entries[emitter_index];
    placement = entry->particle_ribbon.placement_flags;
    flags = entry->particle_ribbon.flags;
    ot = gfx_get_battle_otag();

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;

    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            effect_heap_free_block(record->work_slots[callback_slot]);
        }
        work = effect_heap_alloc_block(sizeof(effect_particle_ribbon_long_work_t), state_index);
        record->work_slots[callback_slot] = work;

        configuration = entry->particle_ribbon.texture_page_config;
        translucent = 1;
        if (configuration.clear_abe) {
            translucent = 0;
        }
        tpage = (configuration.blend_mode << EFFECT_RENDER_TEXTURE_PAGE_BLEND_SHIFT) | EFFECT_RENDER_TEXTURE_PAGE_BASE;

        for (k = 0; k < EFFECT_PARTICLE_RIBBON_LONG_SLOTS; k++) {
            for (i = 0; i < (EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1); i++) {
                quad = &work->ribbons[k].quads[0][i];
                SetPolyGT4(quad);
                setSemiTrans(quad, translucent);
                quad->tpage = tpage;
                quad->clut = BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_EFFECT, 0);
                quad = &work->ribbons[k].quads[1][i];
                SetPolyGT4(quad);
                setSemiTrans(quad, translucent);
                quad->tpage = tpage;
                quad->clut = BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_EFFECT, 0);
            }
            work->ribbons[k].active = 0;
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
        frame %= EFFECT_ANIMATION_CURVE_FRAMES;
        work = record->work_slots[callback_slot];
        target_bindings = entry->particle_ribbon.target_curve_bindings;
        target_index = record->timeline.target_index;
        spawn_bindings = entry->particle_ribbon.spawn_curve_bindings;

        /* Spawn a batch every spawn interval, starting at frame 1. */
        EFFECT_READ_CURVE(j, factor, (target_bindings >> EFFECT_EMITTER_CURVE_SPAWN_INTERVAL_SHIFT) & 0xf, frame);
        if (i
                % effect_lerp_q8(
                    entry->particle_ribbon.spawn_interval_start, entry->particle_ribbon.spawn_interval_end, factor)
            == 0) {
            /* work->matrix keeps a zero translation, so the spawn RotTrans calls only rotate. */
            SetTransMatrix(&work->matrix);
            rotate = (flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING;

            /* The caster's facing as (quadrant + 1) quarter turns of yaw; left unset for a non-unit caster. */
            caster_target = &g_effect_context.targets[EFFECT_CASTER_ENTRY];
            if (caster_target->target_type == EFFECT_TARGET_TYPE_UNIT) {
                quadrant = unit_facing_get_quadrant_by_runtime_id(
                    g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id);
                quadrant = ((quadrant + 1) & 3) << 10;
                vector.vz = 0;
                vector.vx = 0;
                vector.vy = quadrant;
                RotMatrix(&vector, &facing);
            }

            effect_resolve_target_world_coords(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
                EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &caster);
            effect_resolve_target_world_coords(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
                target_index, &g_effect_context, &target);

            EFFECT_READ_CURVE(j, factor, (target_bindings >> EFFECT_EMITTER_CURVE_PARTICLE_COUNT_SHIFT) & 0xf, frame);
            count = effect_lerp_q8(
                entry->particle_ribbon.particle_count_start, entry->particle_ribbon.particle_count_end, factor);

            EFFECT_READ_CURVE(j, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_SPAWN_POSITION_SHIFT) & 0xf, frame);
            effect_interpolate_emitter_spawn_position(entry, factor, &origin);
            switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
            case EFFECT_EMITTER_ORIGIN_WORLD:
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
            case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
                break;
            case EFFECT_EMITTER_ORIGIN_MAP_CENTER:
                battle_map_store_dimensions(&map_origin);
                origin.vx += map_origin.vx * (BATTLE_MAP_TILE_SIZE / 2);
                origin.vz += map_origin.vz * (BATTLE_MAP_TILE_SIZE / 2);
                break;
            }

            EFFECT_READ_CURVE(j, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_SPAWN_SPREAD_SHIFT) & 0xf, frame);
            effect_interpolate_emitter_spawn_spread(entry, factor, &spread);
            if (rotate) {
                SetRotMatrix(&facing);
                vector.vx = spread.vx;
                vector.vy = spread.vy;
                vector.vz = spread.vz;
                RotTrans(&vector, &spread, &result.pad);
            }

            j = (spawn_bindings >> EFFECT_SPAWN_CURVE_RADIAL_SPEED_SHIFT) & 0xf;
            j--;
            if (j != -1) {
                factor = g_effect_animation_curve_data->curves[j][frame];
                speed_min = effect_lerp_q8(
                    entry->particle_ribbon.radial_speed_start.min, entry->particle_ribbon.radial_speed_end.min, factor);
                speed_max = effect_lerp_q8(
                    entry->particle_ribbon.radial_speed_start.max, entry->particle_ribbon.radial_speed_end.max, factor);
            } else {
                speed_min = entry->particle_ribbon.radial_speed_start.min;
                speed_max = entry->particle_ribbon.radial_speed_start.max;
            }

            EFFECT_READ_CURVE(
                j, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_VELOCITY_DIRECTION_SPREAD_SHIFT) & 0xf, frame);
            effect_interpolate_emitter_velocity_direction_spread(entry, factor, &angle_spread);
            EFFECT_READ_CURVE(j, factor, (spawn_bindings >> EFFECT_SPAWN_CURVE_VELOCITY_BASE_ANGLE_SHIFT) & 0xf, frame);
            effect_interpolate_emitter_velocity_base_angles(entry, factor, &angle_base);

            EFFECT_READ_CURVE(j, factor, (target_bindings >> EFFECT_EMITTER_CURVE_TARGET_OFFSET_SHIFT) & 0xf, frame);
            target_base.vx = effect_lerp_q8(
                entry->particle_ribbon.target_offset_start.x, entry->particle_ribbon.target_offset_end.x, factor);
            target_base.vy = effect_lerp_q8(
                entry->particle_ribbon.target_offset_start.y, entry->particle_ribbon.target_offset_end.y, factor);
            target_base.vz = effect_lerp_q8(
                entry->particle_ribbon.target_offset_start.z, entry->particle_ribbon.target_offset_end.z, factor);
            switch (placement & EFFECT_EMITTER_HOMING_MASK) {
            case EFFECT_EMITTER_HOMING_OFFSET:
                break;
            case EFFECT_EMITTER_HOMING_OFFSET_ALT:
                break;
            case EFFECT_EMITTER_HOMING_MAP_CENTER:
                battle_map_store_dimensions(&map_max);
                target_base.vx += map_max.vx * (BATTLE_MAP_TILE_SIZE / 2);
                target_base.vz += map_max.vz * (BATTLE_MAP_TILE_SIZE / 2);
                break;
            case EFFECT_EMITTER_HOMING_CASTER:
                target_base.vx += caster.vx;
                target_base.vy += caster.vy;
                target_base.vz += caster.vz;
                break;
            case EFFECT_EMITTER_HOMING_TARGET:
                target_base.vx += target.vx;
                target_base.vy += target.vy;
                target_base.vz += target.vz;
                break;
            case EFFECT_EMITTER_HOMING_CURSOR_TILE:
                tile = battle_map_tile_get_pointer(
                    g_effect_context.target_tile.vx, g_effect_context.target_tile.vz, g_effect_context.target_tile.vy);
                target_base.vx += g_effect_context.target_tile.vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
                target_base.vz += g_effect_context.target_tile.vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
                target_base.vy -= tile->height * BATTLE_MAP_HEIGHT_STEP;
                break;
            }

            EFFECT_READ_CURVE(
                j, factor, (target_bindings >> EFFECT_PARTICLE_RIBBON_CURVE_TARGET_SPREAD_SHIFT) & 0xf, frame);
            target_spread.vx = effect_lerp_q8(
                entry->particle_ribbon.target_spread_start_x, entry->particle_ribbon.target_spread_end_x, factor);
            target_spread.vy = effect_lerp_q8(
                entry->particle_ribbon.target_spread_start_y, entry->particle_ribbon.target_spread_end_y, factor);
            target_spread.vz = effect_lerp_q8(
                entry->particle_ribbon.target_spread_start_z, entry->particle_ribbon.target_spread_end_z, factor);

            EFFECT_READ_CURVE(j, factor, (target_bindings >> EFFECT_EMITTER_CURVE_LIFETIME_SHIFT) & 0xf, frame);
            lifetime_min = effect_lerp_q8(
                entry->particle_ribbon.lifetime_start.min, entry->particle_ribbon.lifetime_end.min, factor);
            lifetime_max = effect_lerp_q8(
                entry->particle_ribbon.lifetime_start.max, entry->particle_ribbon.lifetime_end.max, factor);

            /* Start up to count ribbons while free slots remain. */
            for (i = 0; i < count; i++) {
                if (work->count >= EFFECT_PARTICLE_RIBBON_LONG_SLOTS) {
                    break;
                }
                work->count++;
                for (k = 0; k < EFFECT_PARTICLE_RIBBON_LONG_SLOTS; k++) {
                    if (work->ribbons[k].active == 0) {
                        slot = &work->ribbons[k];
                        break;
                    }
                }

                /* A random direction scaled by the spread on each axis: a point on the spread ellipsoid. */
                largest = effect_select_largest_magnitude_s16(spread.vx, spread.vy, spread.vz);
                if (largest != 0) {
                    vector.vx = rand();
                    vector.vy = rand();
                    vector.vz = rand();
                    RotMatrix(&vector, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    vector.vx = largest;
                    vector.vz = 0;
                    vector.vy = 0;
                    gte_RotTrans(&vector, &result, &result.pad);
                    slot->position.vx = (result.vx * spread.vx / largest + origin.vx) * ONE;
                    slot->position.vy = (result.vy * spread.vy / largest + origin.vy) * ONE;
                    slot->position.vz = (result.vz * spread.vz / largest + origin.vz) * ONE;
                } else {
                    slot->position.vx = origin.vx * ONE;
                    slot->position.vy = origin.vy * ONE;
                    slot->position.vz = origin.vz * ONE;
                }

                /* The homing target gets the same treatment with its own spread. */
                largest = effect_select_largest_magnitude_s16(target_spread.vx, target_spread.vy, target_spread.vz);
                if (largest != 0) {
                    vector.vx = rand();
                    vector.vy = rand();
                    vector.vz = rand();
                    RotMatrix(&vector, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    vector.vx = largest;
                    vector.vz = 0;
                    vector.vy = 0;
                    RotTrans(&vector, &result, &result.pad);
                    slot->target.vx = target_base.vx + target_spread.vx * result.vx / largest;
                    slot->target.vy = target_base.vy + target_spread.vy * result.vy / largest;
                    slot->target.vz = target_base.vz + target_spread.vz * result.vz / largest;
                } else {
                    slot->target = target_base;
                }

                /* Speeds are 1/512 unit a frame: << 3, or >> 9 of a 4.12 unit vector, gives 20.12. */
                switch (flags & EFFECT_EMITTER_VELOCITY_MASK) {
                case EFFECT_EMITTER_VELOCITY_ANGLE_DIRECTED:
                    vector.vx = angle_base.vx + RAND_SPREAD(angle_spread.vx);
                    vector.vy = angle_base.vy + RAND_SPREAD(angle_spread.vy);
                    vector.vz = angle_base.vz + RAND_SPREAD(angle_spread.vz);
                    RotMatrix(&vector, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    vector.vz = 0;
                    vector.vx = 0;
                    vector.vy = RAND_BETWEEN(speed_min, speed_max);
                    RotTrans(&vector, &result, &result.pad);
                    slot->velocity.vx = result.vx << 3;
                    slot->velocity.vy = result.vy << 3;
                    slot->velocity.vz = result.vz << 3;
                    break;
                case EFFECT_EMITTER_VELOCITY_CASTER_FACING:
                    vector.vx = angle_base.vx + RAND_SPREAD(angle_spread.vx);
                    vector.vy = angle_base.vy + RAND_SPREAD(angle_spread.vy);
                    vector.vz = angle_base.vz + RAND_SPREAD(angle_spread.vz);
                    RotMatrix(&vector, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    vector.vz = 0;
                    vector.vx = 0;
                    vector.vy = RAND_BETWEEN(speed_min, speed_max);
                    RotTrans(&vector, &result, &result.pad);
                    SetRotMatrix(&facing);
                    vector.vx = result.vx;
                    vector.vy = result.vy;
                    vector.vz = result.vz;
                    RotTrans(&vector, &result, &result.pad);
                    slot->velocity.vx = result.vx << 3;
                    slot->velocity.vy = result.vy << 3;
                    slot->velocity.vz = result.vz << 3;
                    break;
                case EFFECT_EMITTER_VELOCITY_INWARD:
                    result.vx = origin.vx - (slot->position.vx >> 12);
                    result.vy = origin.vy - (slot->position.vy >> 12);
                    result.vz = origin.vz - (slot->position.vz >> 12);
                    if (speed_min == speed_max) {
                        speed = speed_min;
                    } else if (speed_max < speed_min) {
                        speed = rand() % (speed_min - speed_max) + speed_max;
                    } else {
                        speed = rand() % (speed_max - speed_min) + speed_min;
                    }
                    /* A particle spawned on the origin has no direction to it and moves along +y. */
                    if (result.vx != 0 || result.vy != 0 || result.vz != 0) {
                        VectorNormal(&result, &result);
                        slot->velocity.vx = speed * result.vx >> 9;
                        slot->velocity.vy = speed * result.vy >> 9;
                        slot->velocity.vz = speed * result.vz >> 9;
                    } else {
                        slot->velocity.vy = speed << 3;
                        slot->velocity.vz = 0;
                        slot->velocity.vx = 0;
                    }
                    break;
                }

                slot->lifetime = RAND_BETWEEN(lifetime_min, lifetime_max);
                slot->age = 0;
                slot->active = 1;
                slot->frame = 0;
                slot->frame_timer = entry->particle_ribbon.frame_delay;

                /* Both builds seed 9 trail points on the spawn position: all of a short trail, 0-8 of a long one. */
                vector.vx = slot->position.vx >> 12;
                vector.vy = slot->position.vy >> 12;
                vector.vz = slot->position.vz >> 12;
                for (j = 0; j < 9; j++) {
                    slot->trail[j].x = vector.vx;
                    slot->trail[j].y = vector.vy;
                    slot->trail[j].z = vector.vz;
                }
            }
        }

        /* Advance and draw every active ribbon. */
        width = entry->particle_ribbon.texture_width;
        height = entry->particle_ribbon.texture_height;
        depth_base = entry->particle_ribbon.ot_depth;
        row = entry->particle_ribbon.brightness_row;
        for (k = 0; k < EFFECT_PARTICLE_RIBBON_LONG_SLOTS; k++) {
            if (work->ribbons[k].active == 0) {
                continue;
            }
            slot = &work->ribbons[k];
            if (--slot->frame_timer == 0) {
                if (++slot->frame > entry->particle_ribbon.frame_count - 1) {
                    slot->frame = 0;
                }
                slot->frame_timer = entry->particle_ribbon.frame_delay;
            }

            /* The color curves are read at the ribbon's age, not the effect frame. */
            if (entry->particle_ribbon.flags & EFFECT_EMITTER_FLAG_COLOR_CURVES) {
                factor = (entry->particle_ribbon.color_curves & 0xfff);
                red = g_effect_animation_curve_data->curves[(factor >> EFFECT_COLOR_CURVE_RED_SHIFT) & 0xf][slot->age];
                green = g_effect_animation_curve_data
                            ->curves[(factor >> EFFECT_COLOR_CURVE_GREEN_SHIFT) & 0xf][slot->age];
                blue
                    = g_effect_animation_curve_data->curves[(factor >> EFFECT_COLOR_CURVE_BLUE_SHIFT) & 0xf][slot->age];
            } else {
                blue = EFFECT_COLOR_NEUTRAL;
                green = EFFECT_COLOR_NEUTRAL;
                red = EFFECT_COLOR_NEUTRAL;
            }

            /* While homing, the newest point eases from the particle's path towards the target. After
             * arrival the strip's head stays on the arrival point and its tail catches up a point a frame. */
            x = (parent.motion.position.x = slot->position.vx += slot->velocity.vx) >> 12;
            y = (parent.motion.position.y = slot->position.vy += slot->velocity.vy) >> 12;
            z = (parent.motion.position.z = slot->position.vz += slot->velocity.vz) >> 12;
            j = ++slot->age;
            index = j % EFFECT_PARTICLE_RIBBON_LONG_POINTS;
            if (j <= slot->lifetime) {
                slot->trail[index].x = effect_interpolate_s32_cos(x, slot->target.vx, slot->lifetime, j);
                slot->trail[index].y = effect_interpolate_s32_cos(y, slot->target.vy, slot->lifetime, j);
                slot->trail[index].z = effect_interpolate_s32_cos(z, slot->target.vz, slot->lifetime, j);
                point = (EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1);
                segments = (EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1);
                if (j < (EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1)) {
                    segments = j;
                }
                u = entry->particle_ribbon.texture_u;
            } else {
                slot->trail[index].x = slot->target.vx;
                slot->trail[index].y = slot->target.vy;
                slot->trail[index].z = slot->target.vz;
                point = slot->lifetime + (EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1) - j;
                if (point < 0) {
                    point = 0;
                }
                index += point - (EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1);
                segments = point;
                if (index < 0) {
                    index += EFFECT_PARTICLE_RIBBON_LONG_POINTS;
                }
                /* Drop texture columns from the head end as the strip shortens. */
                u = entry->particle_ribbon.texture_u
                    + ((EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1) - point)
                        * (width / (EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1));
            }

            /* Children spawn from the strip's head point. */
            parent.motion.position.x = slot->trail[index].x * ONE;
            parent.motion.position.y = slot->trail[index].y * ONE;
            parent.motion.position.z = slot->trail[index].z * ONE;
            if (flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) {
                effect_particle_spawn_from_emitter(
                    record->record_index, slot->age, entry->particle_ribbon.child_emitter_mid_life, &parent);
            }

            /* Head: the first quad's leading edge, perpendicular to the first segment (angle + 0x400). */
            v = entry->particle_ribbon.texture_v + slot->frame * height;
            quad = &slot->quads[work->buffer][0];
            SetRotMatrix(&g_camera_matrix);
            SetTransMatrix(&g_camera_matrix);
            vector.vx = slot->trail[index].x;
            vector.vy = slot->trail[index].y;
            vector.vz = slot->trail[index].z;
            RotTrans(&vector, &head, &head.pad);
            index--;
            if (index < 0) {
                index = (EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1);
            }
            vector.vx = slot->trail[index].x;
            vector.vy = slot->trail[index].y;
            vector.vz = slot->trail[index].z;
            RotTrans(&vector, &tail, &tail.pad);
            angle = ratan2(tail.vy - head.vy, tail.vx - head.vx);
            offset_x = rcos(angle + 0x400) * height / 2 >> 12;
            offset_y = rsin(angle + 0x400) * height / 2 >> 12;
            depth = (head.vz + tail.vz) / 8 + depth_base;
            quad->x0 = head.vx - offset_x;
            quad->y0 = head.vy - offset_y;
            quad->x2 = head.vx + offset_x;
            quad->y2 = head.vy + offset_y;
            quad->u0 = quad->u2 = u;
            quad->v0 = v;
            quad->v2 = v + height;
            factor = g_effect_particle_ribbon_long_brightness_table[row][point];
            setRGB0(
                quad, quad->r2 = factor * red / ONE, quad->g2 = factor * green / ONE, quad->b2 = factor * blue / ONE);
            point--;

            /* Each joint is mitered along the bisector of its two segments. Averaging angles more than
             * half a turn apart points the wrong way, so 0xc00 replaces 0x400 there. */
            for (j = 1; j < segments; j++) {
                index--;
                prev = quad;
                quad = &slot->quads[work->buffer][j];
                head = tail;
                if (index < 0) {
                    index = (EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1);
                }
                vector.vx = slot->trail[index].x;
                vector.vy = slot->trail[index].y;
                vector.vz = slot->trail[index].z;
                RotTrans(&vector, &tail, &tail.pad);
                next_angle = ratan2(tail.vy - head.vy, tail.vx - head.vx);
                bisector = (next_angle + angle) / 2;
                mid = bisector + 0x400;
                if (next_angle - angle < -0x800 || next_angle - angle > 0x800) {
                    mid = bisector + 0xc00;
                }
                offset_x = rcos(mid) * height / 2 >> 12;
                offset_y = rsin(mid) * height / 2 >> 12;
                prev->x1 = quad->x0 = head.vx - offset_x;
                prev->y1 = quad->y0 = head.vy - offset_y;
                prev->x3 = quad->x2 = head.vx + offset_x;
                prev->y3 = quad->y2 = head.vy + offset_y;
                column_width = width / (EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1);
                prev->u1 = prev->u3 = quad->u0 = quad->u2 = u + j * column_width;
                prev->v1 = quad->v0 = v;
                prev->v3 = quad->v2 = v + height;
                factor = g_effect_particle_ribbon_long_brightness_table[row][point];
                setRGB1(prev, prev->r3 = quad->r0 = quad->r2 = factor * red / ONE,
                    prev->g3 = quad->g0 = quad->g2 = factor * green / ONE,
                    prev->b3 = quad->b0 = quad->b2 = factor * blue / ONE);
                point--;

                /* The previous quad is now complete: link it at its segment's depth. */
                if (depth < EFFECT_OT_DEPTH_MIN) {
                    depth = EFFECT_OT_DEPTH_MIN;
                }
                if (depth >= EFFECT_OT_DEPTH_LIMIT) {
                    depth = EFFECT_OT_DEPTH_MAX;
                }
                AddPrim(ot + depth, prev);
                depth = (head.vz + tail.vz) / 8 + depth_base;
                angle = next_angle;
            }

            /* Tail: the last quad's trailing edge, perpendicular to the last segment. */
            offset_x = rcos(angle + 0x400) * height / 2 >> 12;
            offset_y = rsin(angle + 0x400) * height / 2 >> 12;
            quad->x1 = tail.vx - offset_x;
            quad->y1 = tail.vy - offset_y;
            quad->x3 = tail.vx + offset_x;
            quad->y3 = tail.vy + offset_y;
            quad->u1 = quad->u3 = u + width / (EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1) * segments;
            quad->v1 = v;
            quad->v3 = v + height;
            factor = g_effect_particle_ribbon_long_brightness_table[row][point];
            setRGB1(
                quad, quad->r3 = factor * red / ONE, quad->g3 = factor * green / ONE, quad->b3 = factor * blue / ONE);

            if (depth < EFFECT_OT_DEPTH_MIN) {
                depth = EFFECT_OT_DEPTH_MIN;
            }
            if (depth >= EFFECT_OT_DEPTH_LIMIT) {
                depth = EFFECT_OT_DEPTH_MAX;
            }
            AddPrim(ot + depth, quad);

            /* Done once the tail reaches the target: the lifetime plus a frame per trailing segment. */
            if (slot->lifetime + (EFFECT_PARTICLE_RIBBON_LONG_POINTS - 1) == slot->age) {
                if (flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK) {
                    effect_particle_spawn_from_emitter(
                        record->record_index, slot->age, entry->particle_ribbon.child_emitter_on_death, &parent);
                }
                slot->active = 0;
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
