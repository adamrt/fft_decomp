/* A spinning stack of eight textured rings with scrolling texture and brightness by row. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_effect_particle.h"
#include "fft/battle_effect_projectile.h"
#include "fft/battle_effect_script.h"
#include "fft/battle_map.h"
#include "fft/effect.h"
#include "fft/main_gfx.h"
#include "fft/map.h"
#include "psx/gte_inline.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

enum {
    RING_STACK_RING_COUNT = 8,
    RING_STACK_SEGMENT_COUNT = 8,
    RING_STACK_LATTICE_ROW_COUNT = RING_STACK_RING_COUNT + 1,
    RING_STACK_QUAD_COUNT = RING_STACK_RING_COUNT * RING_STACK_SEGMENT_COUNT,
    RING_STACK_LAST_SEGMENT = RING_STACK_SEGMENT_COUNT - 1,
    RING_STACK_LAST_RING_OFFSET = (RING_STACK_RING_COUNT - 1) * RING_STACK_SEGMENT_COUNT,
};

/* Draws a spinning stack of eight textured rings using alternating quad
 * buffers, scrolling texture and per-row brightness. Separate screen vectors
 * and chained color stores preserve the renderer's register scheduling. */
s32 effect_ring_stack_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame) {
    s16 state_index = record_index;
    SVECTOR vertex;
    SVECTOR unused_18[3]; /* frame padding the target reserves but never touches */
    VECTOR growth;
    VECTOR growth_step;
    VECTOR points[RING_STACK_LATTICE_ROW_COUNT][RING_STACK_SEGMENT_COUNT];
    VECTOR screen0;
    VECTOR screen1;
    VECTOR screen2;
    VECTOR screen3;
    s16 depth[RING_STACK_LATTICE_ROW_COUNT + 1][RING_STACK_SEGMENT_COUNT]; /* row 9 is never written */
    VECTOR origin;
    VECTOR unused_5c0[3]; /* frame padding */
    SVECTOR map_max;
    effect_ring_mesh_work_t* work;
    const effect_geometry_record_t* entry;
    s32 radius;
    s32 u;
    s32 v;
    s32 u_span;
    s32 v_span;
    s32 buffer;
    u32* ot;
    effect_record_t* record;
    POLY_GT4* quad;
    POLY_GT4* other;
    POLY_GT4* quads;
    u16 placement;
    s32 translucent;
    u16 tpage;
    effect_texture_page_config_t configuration;
    u8 target_index;
    s32 j;
    s32 texture_selector;
    s32 growth_step_selector;
    u16 radius_binding;
    s32 i;
    s32 k;
    s32 n;
    s32 sin0; /* rsin(spin) */
    s32 sin1; /* rsin(spin + 45 degrees) */
    s32 cos0;
    s32 cos1;
    s32 origin_x;
    s32 origin_y;
    s32 origin_z;
    u32 blue;
    u32 green;
    u32 red;
    map_tile_t* tile;

    record = &g_battle_effect_state_records[state_index];
    entry = &g_battle_effect_geometry_table->entries[emitter_index];
    placement = entry->radial_mesh.placement_flags;
    ot = main_gfx_get_otag();

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;

    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            battle_heap_free_block(record->work_slots[callback_slot]);
        }
        work = battle_heap_alloc_block(sizeof(effect_ring_mesh_work_t), state_index);
        record->work_slots[callback_slot] = work;
        configuration = entry->radial_mesh.texture_page_config;
        tpage = (configuration.blend_mode << EFFECT_RENDER_TEXTURE_PAGE_BLEND_SHIFT) | EFFECT_RENDER_TEXTURE_PAGE_BASE;
        translucent = 1;
        if (configuration.clear_abe) {
            translucent = 0;
        }
        for (j = 0; j < 2; j++) {
            for (i = 0; i < RING_STACK_QUAD_COUNT; i++) {
                quad = &work->quads[j][i];
                SetPolyGT4(quad);
                setSemiTrans(quad, translucent);
                quad->tpage = tpage;
                quad->clut = MAP_CLUT(MAP_CLUT_ROW_EFFECT, 0);
            }
        }
        work->spin = 0;
        work->v_scroll = 0;
        work->buffer = 0;
        work->radius_offset = 0;
        work->radius_speed = 0;
        work->destroy_delay = 0;
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;

    case EFFECT_CALLBACK_PHASE_UPDATE:
        work = record->work_slots[callback_slot];
        target_index = g_battle_effect_state_records[state_index].timeline.target_index;
        EFFECT_READ_CURVE(i, j, entry->radial_mesh.motion_curves.origin, frame);
        battle_effect_interpolate_emitter_spawn_position(entry, j, &origin);
        switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
        case EFFECT_EMITTER_ORIGIN_WORLD:
            break;
        case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
            tile = battle_map_get_tile_data_pointer(g_battle_effect_target_tile.data.tile.vx,
                g_battle_effect_target_tile.data.tile.vz, g_battle_effect_target_tile.data.tile.vy);
            origin.vx += g_battle_effect_target_tile.data.tile.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
            origin.vy -= tile->height * MAP_HEIGHT_STEP;
            origin.vz += g_battle_effect_target_tile.data.tile.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
            break;
        case EFFECT_EMITTER_ORIGIN_CASTER:
            battle_effect_resolve_target_world_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, EFFECT_TARGET_INDEX_CASTER,
                &g_battle_effect_context, &vertex);
            origin.vx += vertex.vx;
            origin.vy += vertex.vy;
            origin.vz += vertex.vz;
            break;
        case EFFECT_EMITTER_ORIGIN_TARGET:
            battle_effect_resolve_target_world_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, target_index,
                &g_battle_effect_context, &vertex);
            origin.vx += vertex.vx;
            origin.vy += vertex.vy;
            origin.vz += vertex.vz;
            break;
        case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
            break;
        case EFFECT_EMITTER_ORIGIN_MAP_CENTER:
            battle_map_store_max_coordinates(&map_max);
            origin.vx += map_max.vx * (MAP_TILE_SIZE / 2);
            origin.vz += map_max.vz * (MAP_TILE_SIZE / 2);
            break;
        }

        radius_binding = entry->radial_mesh.shape_curves.radius;
        EFFECT_READ_CURVE(i, j, radius_binding, frame);
        radius = battle_lerp_fixed8(entry->radial_mesh.radius_start, entry->radial_mesh.radius_end, j);

        texture_selector = entry->radial_mesh.motion_curves.texture;
        EFFECT_READ_CURVE(i, j, texture_selector, frame);
        u = battle_lerp_fixed8(entry->radial_mesh.u_start, entry->radial_mesh.u_end, j);
        v = battle_lerp_fixed8(entry->radial_mesh.v_start, entry->radial_mesh.v_end, j);
        u_span = entry->radial_mesh.u_span;
        v_span = entry->radial_mesh.v_span;
        work->v_scroll += entry->radial_mesh.v_scroll_speed;
        if (work->v_scroll > v_span << 8) {
            work->v_scroll -= v_span << 8;
        }
        if (work->v_scroll < 0) {
            work->v_scroll += v_span << 8;
        }

        EFFECT_READ_CURVE(i, j, entry->radial_mesh.shape_curves.growth, frame);
        growth.vx = battle_lerp_fixed8(entry->radial_mesh.growth_start, entry->radial_mesh.growth_end, j);
        growth.vy = battle_lerp_fixed8(entry->radial_mesh.height_start, entry->radial_mesh.height_end, j);
        growth.vz = battle_lerp_fixed8(entry->radial_mesh.row_radius_start, entry->radial_mesh.row_radius_end, j);

        growth_step_selector = entry->radial_mesh.shape_curves.growth_step;
        EFFECT_READ_CURVE(i, j, growth_step_selector, frame);
        growth_step.vx
            = battle_lerp_fixed8(entry->radial_mesh.growth_step_start, entry->radial_mesh.growth_step_end, j);
        growth_step.vy
            = battle_lerp_fixed8(entry->radial_mesh.height_step_start, entry->radial_mesh.height_step_end, j);
        growth_step.vz
            = battle_lerp_fixed8(entry->radial_mesh.row_radius_step_start, entry->radial_mesh.row_radius_step_end, j);

        EFFECT_READ_CURVE(i, j, entry->radial_mesh.motion_curves.spin, frame);
        work->spin
            = (work->spin
                  + battle_lerp_fixed8(entry->radial_mesh.spin_speed_start, entry->radial_mesh.spin_speed_end, j))
            & (ONE - 1);
        sin0 = rsin(work->spin);
        sin1 = rsin(work->spin + (ONE / RING_STACK_SEGMENT_COUNT));
        cos0 = rcos(work->spin);
        cos1 = rcos(work->spin + (ONE / RING_STACK_SEGMENT_COUNT));

        n = 0;
        k = 0;
        for (i = 0; i < RING_STACK_LATTICE_ROW_COUNT; i++) {
            points[i][0].vx = points[i][2].vz = (cos0 * (radius + ((k + work->radius_offset) >> 8))) >> 12;
            points[i][4].vx = points[i][6].vz = -points[i][0].vx;
            points[i][0].vz = points[i][6].vx = (sin0 * (radius + ((k + work->radius_offset) >> 8))) >> 12;
            points[i][2].vx = points[i][4].vz = -points[i][0].vz;
            points[i][1].vx = points[i][3].vz = (cos1 * (radius + ((k + work->radius_offset) >> 8))) >> 12;
            points[i][5].vx = points[i][7].vz = -points[i][1].vx;
            points[i][1].vz = points[i][7].vx = (sin1 * (radius + ((k + work->radius_offset) >> 8))) >> 12;
            points[i][3].vx = points[i][5].vz = -points[i][1].vz;
            points[i][0].vy = points[i][1].vy = points[i][2].vy = points[i][3].vy = points[i][4].vy = points[i][5].vy
                = points[i][6].vy = points[i][7].vy = n >> 8;
            growth.vy += growth_step.vy;
            growth.vz += growth_step.vz;
            n += growth.vy;
            k += growth.vz;
        }
        work->radius_speed += growth_step.vx;
        work->radius_offset += growth.vx + work->radius_speed;

        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        origin_x = origin.vx;
        buffer = work->buffer;
        origin_y = origin.vy;
        origin_z = origin.vz;

        /* Ring 0: quad i - 1 takes lattice rows 1 (x0/x1) and 0 (x2/x3); quad 7
         * closes the ring on quad 0. */
        vertex.vx = points[0][0].vx + origin_x;
        vertex.vy = points[0][0].vy + origin_y;
        vertex.vz = points[0][0].vz + origin_z;
        gte_RotTrans_split(&vertex, &screen2, &screen2.pad);
        quad = &work->quads[buffer][0];
        quad->x2 = screen2.vx;
        quad->y2 = screen2.vy;
        depth[0][0] = screen2.vz;
        vertex.vx = points[0][1].vx + origin_x;
        vertex.vy = points[0][1].vy + origin_y;
        vertex.vz = points[0][1].vz + origin_z;
        gte_RotTrans_split(&vertex, &screen3, &screen3.pad);
        quad->x3 = screen3.vx;
        quad->y3 = screen3.vy;
        depth[0][1] = screen3.vz;
        vertex.vx = points[1][0].vx + origin_x;
        vertex.vy = points[1][0].vy + origin_y;
        vertex.vz = points[1][0].vz + origin_z;
        gte_RotTrans_split(&vertex, &screen0, &screen0.pad);
        quad->x0 = screen0.vx;
        quad->y0 = screen0.vy;
        depth[1][0] = screen0.vz;
        vertex.vx = points[1][1].vx + origin_x;
        vertex.vy = points[1][1].vy + origin_y;
        vertex.vz = points[1][1].vz + origin_z;
        gte_RotTrans_split(&vertex, &screen1, &screen1.pad);
        quad->x1 = screen1.vx;
        quad->y1 = screen1.vy;
        depth[1][1] = screen1.vz;

        for (i = 2; i < RING_STACK_SEGMENT_COUNT; i++) {
            quad = &work->quads[buffer][i - 1];
            quad->x0 = screen1.vx;
            quad->y0 = screen1.vy;
            quad->x2 = screen3.vx;
            quad->y2 = screen3.vy;
            vertex.vx = points[0][i].vx + origin_x;
            vertex.vy = points[0][i].vy + origin_y;
            vertex.vz = points[0][i].vz + origin_z;
            gte_RotTrans_split(&vertex, &screen3, &screen3.pad);
            quad->x3 = screen3.vx;
            quad->y3 = screen3.vy;
            depth[0][i] = screen3.vz;
            vertex.vx = points[1][i].vx + origin_x;
            vertex.vy = points[1][i].vy + origin_y;
            vertex.vz = points[1][i].vz + origin_z;
            gte_RotTrans_split(&vertex, &screen1, &screen1.pad);
            quad->x1 = screen1.vx;
            quad->y1 = screen1.vy;
            depth[1][i] = screen1.vz;
        }

        quads = work->quads[buffer];
        quads[RING_STACK_LAST_SEGMENT].x0 = screen1.vx;
        quads[RING_STACK_LAST_SEGMENT].y0 = screen1.vy;
        quads[RING_STACK_LAST_SEGMENT].x2 = screen3.vx;
        quads[RING_STACK_LAST_SEGMENT].y2 = screen3.vy;
        other = quads;
        quads[RING_STACK_LAST_SEGMENT].x1 = other->x0;
        quads[RING_STACK_LAST_SEGMENT].y1 = other->y0;
        quads[RING_STACK_LAST_SEGMENT].x3 = other->x2;
        quads[RING_STACK_LAST_SEGMENT].y3 = other->y2;

        /* Ring j reuses the lower corners of ring j - 1 and adds lattice row
         * j + 1. */
        for (j = 1; j < RING_STACK_RING_COUNT; j++) {
            n = j + 1;

            vertex.vx = points[n][0].vx + origin_x;
            vertex.vy = points[n][0].vy + origin_y;
            vertex.vz = points[n][0].vz + origin_z;
            gte_RotTrans_split(&vertex, &screen0, &screen0.pad);
            quad = &work->quads[buffer][j * RING_STACK_SEGMENT_COUNT];
            quad->x0 = screen0.vx;
            quad->y0 = screen0.vy;
            depth[n][0] = screen0.vz;
            vertex.vx = points[n][1].vx + origin_x;
            vertex.vy = points[n][1].vy + origin_y;
            vertex.vz = points[n][1].vz + origin_z;
            gte_RotTrans_split(&vertex, &screen1, &screen1.pad);
            quad->x1 = screen1.vx;
            quad->y1 = screen1.vy;
            depth[n][1] = screen1.vz;
            other = &work->quads[buffer][(j - 1) * RING_STACK_SEGMENT_COUNT];
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            for (i = 2; i < RING_STACK_SEGMENT_COUNT; i++) {
                quad = &work->quads[buffer][j * RING_STACK_SEGMENT_COUNT + i - 1];
                quad->x0 = screen1.vx;
                quad->y0 = screen1.vy;
                other = &work->quads[buffer][(j - 1) * RING_STACK_SEGMENT_COUNT + i - 1];
                quad->x2 = other->x0;
                quad->y2 = other->y0;
                quad->x3 = other->x1;
                quad->y3 = other->y1;
                vertex.vx = points[n][i].vx + origin_x;
                vertex.vy = points[n][i].vy + origin_y;
                vertex.vz = points[n][i].vz + origin_z;
                gte_RotTrans_split(&vertex, &screen1, &screen1.pad);
                quad->x1 = screen1.vx;
                quad->y1 = screen1.vy;
                depth[n][i] = screen1.vz;
            }
            quad = &work->quads[buffer][j * RING_STACK_SEGMENT_COUNT + RING_STACK_LAST_SEGMENT];
            other = &work->quads[buffer][(j - 1) * RING_STACK_SEGMENT_COUNT + RING_STACK_LAST_SEGMENT];
            quad->x0 = screen1.vx;
            quad->y0 = screen1.vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            other = &work->quads[buffer][j * RING_STACK_SEGMENT_COUNT];
            quad->x1 = other->x0;
            quad->y1 = other->y0;
        }

        j = work->v_scroll >> 8;
        for (i = 0; i < RING_STACK_QUAD_COUNT; i++) {
            quad = &work->quads[buffer][i];
            quad->u0 = quad->u2 = u;
            quad->v0 = quad->v1 = v + j;
            quad->u1 = quad->u3 = u + u_span;
            quad->v2 = quad->v3 = v + v_span + j;
        }

        n = entry->radial_mesh.brightness_row;
        blue = EFFECT_COLOR_NEUTRAL;
        if (entry->radial_mesh.flags & EFFECT_EMITTER_FLAG_COLOR_CURVES) {
            k = (entry->radial_mesh.color_curves & 0xfff);
            red = g_battle_effect_animation_curve_data->curves[(k >> EFFECT_COLOR_CURVE_RED_SHIFT) & 0xf][frame];
            green = g_battle_effect_animation_curve_data->curves[(k >> EFFECT_COLOR_CURVE_GREEN_SHIFT) & 0xf][frame];
            blue = g_battle_effect_animation_curve_data->curves[(k >> EFFECT_COLOR_CURVE_BLUE_SHIFT) & 0xf][frame];
        } else {
            green = EFFECT_COLOR_NEUTRAL;
            red = EFFECT_COLOR_NEUTRAL;
        }
        k = g_effect_ring_stack_mesh_brightness_table[n][0];
        for (i = 0; i < RING_STACK_SEGMENT_COUNT; i++) {
            quad = &work->quads[buffer][i];
            quad->r3 = (k * red) / ONE;
            quad->r2 = (k * red) / ONE;
            quad->g3 = (k * green) / ONE;
            quad->g2 = (k * green) / ONE;
            quad->b3 = (k * blue) / ONE;
            quad->b2 = (k * blue) / ONE;
        }
        for (j = 0; j < RING_STACK_RING_COUNT - 1; j++) {
            k = g_effect_ring_stack_mesh_brightness_table[n][j + 1];
            for (i = 0; i < RING_STACK_SEGMENT_COUNT; i++) {
                quad = &work->quads[buffer][j * RING_STACK_SEGMENT_COUNT + i];
                other = &work->quads[buffer][(j + 1) * RING_STACK_SEGMENT_COUNT + i];
                setRGB0(quad, quad->r1 = other->r2 = other->r3 = (k * red) / ONE,
                    quad->g1 = other->g2 = other->g3 = (k * green) / ONE,
                    quad->b1 = other->b2 = other->b3 = (k * blue) / ONE);
            }
        }
        k = g_effect_ring_stack_mesh_brightness_table[n][RING_STACK_RING_COUNT];
        for (i = 0; i < RING_STACK_SEGMENT_COUNT; i++) {
            quad = &work->quads[buffer][i + RING_STACK_LAST_RING_OFFSET];
            quad->r1 = (k * red) / ONE;
            quad->r0 = (k * red) / ONE;
            quad->g1 = (k * green) / ONE;
            quad->g0 = (k * green) / ONE;
            quad->b1 = (k * blue) / ONE;
            quad->b0 = (k * blue) / ONE;
        }

        for (j = 0; j < RING_STACK_RING_COUNT; j++) {
            for (i = 0; i < RING_STACK_LAST_SEGMENT; i++) {
                n = (depth[j][i] + depth[j][i + 1] + depth[j + 1][i] + depth[j + 1][i + 1]) / 16;
                if (n >= 0 && n < EFFECT_OT_DEPTH_LIMIT) {
                    addPrim(&ot[n], &work->quads[buffer][j * RING_STACK_SEGMENT_COUNT + i]);
                }
            }
            n = (depth[j][0] + depth[j][RING_STACK_LAST_SEGMENT] + depth[j + 1][0]
                    + depth[j + 1][RING_STACK_LAST_SEGMENT])
                / 16;
            if (n >= 0 && n < EFFECT_OT_DEPTH_LIMIT) {
                addPrim(&ot[n], &work->quads[buffer][j * RING_STACK_SEGMENT_COUNT + RING_STACK_LAST_SEGMENT]);
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
