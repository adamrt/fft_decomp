#include "fft/battle.h"
#include "fft/effect_particle_ribbon.h"
#include "fft/effect_summon_mesh.h"
#include "fft/map.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"

#define RIBBON_SLOTS    EFFECT_PARTICLE_RIBBON_SHORT_SLOTS
#define RIBBON_POINTS   EFFECT_PARTICLE_RIBBON_SHORT_POINTS
#define RIBBON_SEGMENTS (RIBBON_POINTS - 1)

/* Spawns homing particles and draws each one's recent path as a ribbon of
 * textured quads.
 *
 * Every spawn interval up to RIBBON_SLOTS ribbons start from the emitter's
 * spawn volume with a randomised heading and speed (optionally turned to the
 * caster's facing) and ease towards a spread target over their lifetime. Each
 * update appends the particle's position to a RIBBON_POINTS ring buffer and
 * rebuilds the double-buffered quad strip along it, mitred at each joint.
 *
 * Matching notes: `quadrant` is assigned twice so the scheduler keeps the
 * RotMatrix arguments after the angle arithmetic; `point` and `index` are spelled
 * so fold keeps the target's (j - 16) and (index - 16) subexpressions; the
 * head point is re-read through the same `slot->trail[index]` address before
 * `index` steps back to the tail. */
s32 effect_e355_update_particle_ribbons_short_state(s16 record_index, s32 byte_offset, s32 geometry_index, s32 frame) {
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
    effect_particle_ribbon_record_view_t* record;
    effect_particle_ribbon_short_work_t* work;
    effect_particle_ribbon_geometry_view_t* entry;
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
    s32 height;
    s32 depth_base;
    s16 angle;
    s16 next_angle;
    u32 red;
    u32 green;
    u32 blue;
    u32* ot;
    u16 placement;
    s32 wave;
    s32 scroll;
    s32 target_index;
    s32 factor;
    s32 rotate;
    s16 quadrant;
    s32 abe;
    s32 translucent;
    u16 tpage;
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
    effect_particle_ribbon_short_t* slot;
    POLY_GT4* quad;
    POLY_GT4* prev;
    map_tile_t* tile;
    battle_effect_target_t* caster_target;

    record = (effect_particle_ribbon_record_view_t*)&g_effect_state_records[record_index];
    entry = (effect_particle_ribbon_geometry_view_t*)&g_effect_geometry_table->entries[geometry_index];
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
        work = battle_heap_alloc_block(sizeof(effect_particle_ribbon_short_work_t), record_index);
        record->work_slots[byte_offset] = work;
        abe = entry->texture_page_flags & 4;
        translucent = abe == 0;
        tpage = ((entry->texture_page_flags & 3) << 5) | 0x86;
        for (k = 0; k < RIBBON_SLOTS; k++) {
            for (i = 0; i < RIBBON_SEGMENTS; i++) {
                quad = &work->ribbons[k].quads[0][i];
                SetPolyGT4(quad);
                if (translucent) {
                    quad->code |= 2;
                } else {
                    quad->code &= ~2;
                }
                quad->tpage = tpage;
                quad->clut = 0x7b00;
                quad = &work->ribbons[k].quads[1][i];
                SetPolyGT4(quad);
                if (translucent) {
                    quad->code |= 2;
                } else {
                    quad->code &= ~2;
                }
                quad->tpage = tpage;
                quad->clut = 0x7b00;
            }
            work->ribbons[k].active = 0;
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
        frame %= 160;
        work = record->work_slots[byte_offset];
        wave = entry->curve_indices[1];
        target_index = record->target_index;
        scroll = entry->curve_indices[0];
        READ_CURVE(factor, j, (wave >> 24) & 0xf, frame);
        if (i % battle_effect_lerp_linear(entry->spawn_interval_start, entry->spawn_interval_end, factor) == 0) {
            SetTransMatrix(&work->matrix);
            rotate = (flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING;
            caster_target = &g_battle_effect_coord_data.targets[16];
            if (caster_target->target_type == 0) {
                quadrant
                    = battle_unit_get_facing_quadrant_by_misc_id(g_battle_effect_coord_data.targets[16].id.misc_id);
                quadrant = ((quadrant + 1) & 3) << 10;
                vector.vz = 0;
                vector.vx = 0;
                vector.vy = quadrant;
                RotMatrix(&vector, &facing);
            }
            battle_effect_resolve_target_render_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, -1,
                &g_battle_effect_coord_data, &caster);
            battle_effect_resolve_target_render_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, target_index,
                &g_battle_effect_coord_data, &target);

            READ_CURVE(factor, j, (wave >> 20) & 0xf, frame);
            count = battle_effect_lerp_linear(entry->particle_count_start, entry->particle_count_end, factor);

            READ_CURVE(factor, j, scroll & 0xf, frame);
            battle_effect_interpolate_emitter_spawn_position(entry, factor, &origin.vx);
            switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
            case EFFECT_EMITTER_ORIGIN_WORLD:
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
            case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
                break;
            case EFFECT_EMITTER_ORIGIN_MAP_CENTRE:
                battle_map_store_max_coordinates(&map_origin);
                origin.vx += map_origin.vx * 14;
                origin.vz += map_origin.vz * 14;
                break;
            }

            READ_CURVE(factor, j, (scroll >> 4) & 0xf, frame);
            battle_effect_interpolate_emitter_spawn_spread(entry, factor, &spread.vx);
            if (rotate) {
                SetRotMatrix(&facing);
                vector.vx = spread.vx;
                vector.vy = spread.vy;
                vector.vz = spread.vz;
                RotTrans(&vector, &spread, &result.pad);
            }

            j = (u32)scroll >> 28;
            j--;
            if (j != -1) {
                factor = g_effect_palette_table[j].factor[frame];
                speed_min
                    = battle_effect_lerp_linear(entry->radial_speed_min_start, entry->radial_speed_min_end, factor);
                speed_max
                    = battle_effect_lerp_linear(entry->radial_speed_max_start, entry->radial_speed_max_end, factor);
            } else {
                speed_min = entry->radial_speed_min_start;
                speed_max = entry->radial_speed_max_start;
            }

            READ_CURVE(factor, j, (scroll >> 12) & 0xf, frame);
            battle_effect_interpolate_emitter_velocity_direction_spread(entry, factor, &angle_spread.vx);
            READ_CURVE(factor, j, (scroll >> 8) & 0xf, frame);
            battle_effect_interpolate_emitter_velocity_base_angles(entry, factor, &angle_base.vx);

            READ_CURVE(factor, j, (wave >> 12) & 0xf, frame);
            target_base.vx
                = battle_effect_lerp_linear(entry->target_offset_start[0], entry->target_offset_end[0], factor);
            target_base.vy
                = battle_effect_lerp_linear(entry->target_offset_start[1], entry->target_offset_end[1], factor);
            target_base.vz
                = battle_effect_lerp_linear(entry->target_offset_start[2], entry->target_offset_end[2], factor);
            switch (placement & EFFECT_EMITTER_HOMING_MASK) {
            case EFFECT_EMITTER_HOMING_OFFSET:
                break;
            case EFFECT_EMITTER_HOMING_OFFSET_ALT:
                break;
            case EFFECT_EMITTER_HOMING_MAP_CENTRE:
                battle_map_store_max_coordinates(&map_max);
                target_base.vx += map_max.vx * 14;
                target_base.vz += map_max.vz * 14;
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
                tile = battle_map_get_tile_data_pointer(g_battle_effect_target_tile.id.tile_x,
                    g_battle_effect_target_tile.tile_y, g_battle_effect_target_tile.map_z);
                target_base.vx += g_battle_effect_target_tile.id.tile_x * 28 + 14;
                target_base.vz += g_battle_effect_target_tile.tile_y * 28 + 14;
                target_base.vy -= tile->height * 12;
                break;
            }

            READ_CURVE(factor, j, (wave >> 4) & 0xf, frame);
            target_spread.vx
                = battle_effect_lerp_linear(entry->target_spread_start[0], entry->target_spread_end[0], factor);
            target_spread.vy
                = battle_effect_lerp_linear(entry->target_spread_start[2], entry->target_spread_end[2], factor);
            target_spread.vz
                = battle_effect_lerp_linear(entry->target_spread_start[4], entry->target_spread_end[4], factor);

            READ_CURVE(factor, j, (wave >> 8) & 0xf, frame);
            lifetime_min = battle_effect_lerp_linear(entry->lifetime_start[0], entry->lifetime_end[0], factor);
            lifetime_max = battle_effect_lerp_linear(entry->lifetime_start[1], entry->lifetime_end[1], factor);

            for (i = 0; i < count; i++) {
                if (work->count >= RIBBON_SLOTS) {
                    break;
                }
                work->count++;
                for (k = 0; k < RIBBON_SLOTS; k++) {
                    if (work->ribbons[k].active == 0) {
                        slot = &work->ribbons[k];
                        break;
                    }
                }

                largest = battle_effect_select_largest_magnitude_s16(spread.vx, spread.vy, spread.vz);
                if (largest != 0) {
                    vector.vx = rand();
                    vector.vy = rand();
                    vector.vz = rand();
                    RotMatrix(&vector, &work->matrix);
                    SetRotMatrix(&work->matrix);
                    vector.vx = largest;
                    vector.vz = 0;
                    vector.vy = 0;
                    gte_ldv0(&vector);
                    gte_rtv0tr();
                    gte_stlvnl(&result);
                    gte_stflg(&result.pad);
                    slot->position.vx = (result.vx * spread.vx / largest + origin.vx) << 12;
                    slot->position.vy = (result.vy * spread.vy / largest + origin.vy) << 12;
                    slot->position.vz = (result.vz * spread.vz / largest + origin.vz) << 12;
                } else {
                    slot->position.vx = origin.vx << 12;
                    slot->position.vy = origin.vy << 12;
                    slot->position.vz = origin.vz << 12;
                }

                largest
                    = battle_effect_select_largest_magnitude_s16(target_spread.vx, target_spread.vy, target_spread.vz);
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

                switch (flags & EFFECT_EMITTER_VELOCITY_MASK) {
                case EFFECT_EMITTER_VELOCITY_OUTWARD:
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
                slot->frame_timer = entry->frame_delay;
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

        width = entry->texture_width;
        height = entry->texture_height;
        depth_base = entry->ot_depth;
        row = entry->brightness_row;
        for (k = 0; k < RIBBON_SLOTS; k++) {
            if (work->ribbons[k].active == 0) {
                continue;
            }
            slot = &work->ribbons[k];
            if (--slot->frame_timer == 0) {
                if (++slot->frame > entry->frame_count - 1) {
                    slot->frame = 0;
                }
                slot->frame_timer = entry->frame_delay;
            }
            if (entry->flags & EFFECT_GEOMETRY_FLAG_COLOUR_CURVES) {
                factor = entry->colour_curves & 0xfff;
                red = g_effect_palette_table[entry->colour_curves & 0xf].factor[slot->age];
                green = g_effect_palette_table[(factor >> 4) & 0xf].factor[slot->age];
                blue = g_effect_palette_table[(u32)factor >> 8].factor[slot->age];
            } else {
                blue = 0x80;
                green = 0x80;
                red = 0x80;
            }
            x = (parent.position[0] = slot->position.vx += slot->velocity.vx) >> 12;
            y = (parent.position[1] = slot->position.vy += slot->velocity.vy) >> 12;
            z = (parent.position[2] = slot->position.vz += slot->velocity.vz) >> 12;
            j = ++slot->age;
            index = j % RIBBON_POINTS;
            if (j <= slot->lifetime) {
                slot->trail[index].x = battle_effect_interpolate_s32_cos(x, slot->target.vx, slot->lifetime, j);
                slot->trail[index].y = battle_effect_interpolate_s32_cos(y, slot->target.vy, slot->lifetime, j);
                slot->trail[index].z = battle_effect_interpolate_s32_cos(z, slot->target.vz, slot->lifetime, j);
                point = RIBBON_SEGMENTS;
                segments = RIBBON_SEGMENTS;
                if (j < RIBBON_SEGMENTS) {
                    segments = j;
                }
                u = entry->texture_u;
            } else {
                slot->trail[index].x = slot->target.vx;
                slot->trail[index].y = slot->target.vy;
                slot->trail[index].z = slot->target.vz;
                point = slot->lifetime + RIBBON_SEGMENTS - j;
                if (point < 0) {
                    point = 0;
                }
                index += point - RIBBON_SEGMENTS;
                segments = point;
                if (index < 0) {
                    index += RIBBON_POINTS;
                }
                u = entry->texture_u + (RIBBON_SEGMENTS - point) * (width / RIBBON_SEGMENTS);
            }
            parent.position[0] = slot->trail[index].x << 12;
            parent.position[1] = slot->trail[index].y << 12;
            parent.position[2] = slot->trail[index].z << 12;
            if (flags & 0xc) {
                battle_effect_spawn_emitter_particles(
                    record->record_index, slot->age, entry->child_emitter_mid_life, &parent);
            }
            v = entry->texture_v + slot->frame * height;
            quad = &slot->quads[work->buffer][0];
            SetRotMatrix(&g_battle_camera_matrix);
            SetTransMatrix(&g_battle_camera_matrix);
            vector.vx = slot->trail[index].x;
            vector.vy = slot->trail[index].y;
            vector.vz = slot->trail[index].z;
            RotTrans(&vector, &head, &head.pad);
            index--;
            if (index < 0) {
                index = RIBBON_SEGMENTS;
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
            factor = g_effect_particle_ribbon_short_brightness_table[row][point];
            quad->r0 = quad->r2 = factor * red >> 12;
            quad->g0 = quad->g2 = factor * green >> 12;
            quad->b0 = quad->b2 = factor * blue >> 12;
            point--;
            for (j = 1; j < segments; j++) {
                index--;
                prev = quad;
                quad = &slot->quads[work->buffer][j];
                head = tail;
                if (index < 0) {
                    index = RIBBON_SEGMENTS;
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
                prev->u1 = prev->u3 = quad->u0 = quad->u2 = u + j * (width / RIBBON_SEGMENTS);
                prev->v1 = quad->v0 = v;
                prev->v3 = quad->v2 = v + height;
                factor = g_effect_particle_ribbon_short_brightness_table[row][point];
                prev->r1 = prev->r3 = quad->r0 = quad->r2 = factor * red >> 12;
                prev->g1 = prev->g3 = quad->g0 = quad->g2 = factor * green >> 12;
                prev->b1 = prev->b3 = quad->b0 = quad->b2 = factor * blue >> 12;
                point--;
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
            offset_x = rcos(angle + 0x400) * height / 2 >> 12;
            offset_y = rsin(angle + 0x400) * height / 2 >> 12;
            quad->x1 = tail.vx - offset_x;
            quad->y1 = tail.vy - offset_y;
            quad->x3 = tail.vx + offset_x;
            quad->y3 = tail.vy + offset_y;
            quad->u1 = quad->u3 = u + width / RIBBON_SEGMENTS * segments;
            quad->v1 = v;
            quad->v3 = v + height;
            factor = g_effect_particle_ribbon_short_brightness_table[row][point];
            quad->r1 = quad->r3 = factor * red >> 12;
            quad->g1 = quad->g3 = factor * green >> 12;
            quad->b1 = quad->b3 = factor * blue >> 12;
            if (depth < EFFECT_OT_DEPTH_MIN) {
                depth = EFFECT_OT_DEPTH_MIN;
            }
            if (depth >= EFFECT_OT_DEPTH_LIMIT) {
                depth = EFFECT_OT_DEPTH_MAX;
            }
            AddPrim(ot + depth, quad);
            if (slot->lifetime + RIBBON_SEGMENTS == slot->age) {
                if (flags & 3) {
                    battle_effect_spawn_emitter_particles(
                        record->record_index, slot->age, entry->child_emitter_on_death, &parent);
                }
                slot->active = 0;
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
