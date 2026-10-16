/* A closed tube of textured quads mirrored about its middle ring, with scrolling texture and brightness by row. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_effect_file.h"
#include "fft/battle_effect_geometry.h"
#include "fft/battle_effect_particle.h"
#include "fft/battle_effect_script.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/effect.h"
#include "fft/main_gfx.h"
#include "fft/map.h"
#include "psx/gte_inline.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

enum {
    TUBE_MESH_RING_COUNT = 8,
    TUBE_MESH_SEGMENT_COUNT = 8,
    TUBE_MESH_LATTICE_ROW_COUNT = TUBE_MESH_RING_COUNT + 1,
    TUBE_MESH_QUAD_COUNT = TUBE_MESH_RING_COUNT * TUBE_MESH_SEGMENT_COUNT,
    TUBE_MESH_MIDDLE_ROW = TUBE_MESH_RING_COUNT / 2,
    TUBE_MESH_RING_SCALE_COUNT = TUBE_MESH_MIDDLE_ROW + 1,
    TUBE_MESH_LAST_SEGMENT = TUBE_MESH_SEGMENT_COUNT - 1,
    TUBE_MESH_LAST_RING_OFFSET = (TUBE_MESH_RING_COUNT - 1) * TUBE_MESH_SEGMENT_COUNT,
};

/* Callback handler for the tube mesh: eight rings of eight textured quads
 * around the emitter origin, spinning about the vertical axis. Its nine
 * lattice rows are octagons mirrored about the middle row: rows 4 - i and
 * 4 + i share the radius scale g_effect_tube_mesh_ring_scales[i] and sit at
 * opposite heights, which grow with the radius. The whole radius grows from
 * frame to frame. Each update projects the lattice through the battle camera
 * into the current quad buffer, shades the rows, depth-sorts the quads and
 * flips buffers. Shared temporaries retain the renderer's matched register
 * allocation. */
s32 effect_tube_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame) {
    s16 state_index = record_index;
    SVECTOR vertex;
    SVECTOR unused_18[2]; /* frame padding the target reserves but never touches */
    VECTOR origin;
    VECTOR growth;      /* vx: radius speed step, vy: row height scale */
    VECTOR growth_step; /* vx: radius acceleration, vy: row height scale step */
    VECTOR points[TUBE_MESH_LATTICE_ROW_COUNT][TUBE_MESH_SEGMENT_COUNT];
    VECTOR screen0;
    VECTOR screen1;
    VECTOR screen2;
    VECTOR screen3;
    s16 depth[TUBE_MESH_LATTICE_ROW_COUNT][TUBE_MESH_SEGMENT_COUNT];
    SVECTOR map_max;
    effect_ring_mesh_work_t* work;
    const effect_geometry_record_t* entry;
    s32 radius;
    s32 u;
    s32 v;
    s32 u_span;
    s32 v_span;
    u32* ot;
    effect_record_t* record;
    POLY_GT4* quad;
    POLY_GT4* other;
    u16 placement;
    s32 translucent;
    u16 tpage;
    effect_texture_page_config_t configuration;
    u8 target_index;
    s32 i;
    s32 j;
    s32 texture_selector;
    s32 growth_step_selector;
    u16 radius_binding;
    s32 k;
    s32 n;
    s32 extent;
    s32 sin0;
    s32 sin45;
    s32 cos0;
    s32 cos45;
    s32 origin_x;
    s32 origin_y;
    s32 origin_z;
    u32 blue;
    u32 green;
    u32 red;
    u32 shade_r;
    u32 shade_g;
    u32 shade_b;
    map_tile_t* tile;

    record = &g_battle_effect_state_records[state_index];
    entry = &g_battle_effect_geometry_table->entries[emitter_index];
    placement = entry->radial_mesh.placement_flags;
    ot = gfx_get_battle_otag();

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
            for (i = 0; i < TUBE_MESH_QUAD_COUNT; i++) {
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
        /* Indexed again rather than read through record, as the target does. */
        target_index = g_battle_effect_state_records[state_index].timeline.target_index;

        EFFECT_READ_CURVE(i, j, entry->radial_mesh.motion_curves.origin, frame);
        battle_effect_interpolate_emitter_spawn_position(entry, j, &origin);
        switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
        case EFFECT_EMITTER_ORIGIN_WORLD:
        case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
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
        case EFFECT_EMITTER_ORIGIN_MAP_CENTER:
            battle_map_store_max_coordinates(&map_max);
            origin.vx += map_max.vx * (MAP_TILE_SIZE / 2);
            origin.vz += map_max.vz * (MAP_TILE_SIZE / 2);
            break;
        }

        radius_binding = entry->radial_mesh.shape_curves.radius;
        EFFECT_READ_CURVE(i, j, radius_binding, frame);
        radius = battle_lerp_q8(entry->radial_mesh.radius_start, entry->radial_mesh.radius_end, j);

        texture_selector = entry->radial_mesh.motion_curves.texture;
        EFFECT_READ_CURVE(i, j, texture_selector, frame);
        u = battle_lerp_q8(entry->radial_mesh.u_start, entry->radial_mesh.u_end, j);
        v = battle_lerp_q8(entry->radial_mesh.v_start, entry->radial_mesh.v_end, j);
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
        growth.vx = battle_lerp_q8(entry->radial_mesh.growth_start, entry->radial_mesh.growth_end, j);
        growth.vy = battle_lerp_q8(entry->radial_mesh.height_start, entry->radial_mesh.height_end, j);
        growth.vz = battle_lerp_q8(entry->radial_mesh.row_radius_start, entry->radial_mesh.row_radius_end, j);

        growth_step_selector = entry->radial_mesh.shape_curves.growth_step;
        EFFECT_READ_CURVE(i, j, growth_step_selector, frame);
        growth_step.vx = battle_lerp_q8(entry->radial_mesh.growth_step_start, entry->radial_mesh.growth_step_end, j);
        growth_step.vy = battle_lerp_q8(entry->radial_mesh.height_step_start, entry->radial_mesh.height_step_end, j);
        growth_step.vz
            = battle_lerp_q8(entry->radial_mesh.row_radius_step_start, entry->radial_mesh.row_radius_step_end, j);

        EFFECT_READ_CURVE(i, j, entry->radial_mesh.motion_curves.spin, frame);
        work->spin
            = (work->spin + battle_lerp_q8(entry->radial_mesh.spin_speed_start, entry->radial_mesh.spin_speed_end, j))
            & (ONE - 1);
        sin0 = rsin(work->spin);
        sin45 = rsin(work->spin + (ONE / TUBE_MESH_SEGMENT_COUNT));
        cos0 = rcos(work->spin);
        cos45 = rcos(work->spin + (ONE / TUBE_MESH_SEGMENT_COUNT));

        /* Rows 4 - i and 4 + i share one scaled ring, mirrored above and below the middle row; row
         * heights are proportional to the radius (k). Points 0 and 1 come from the two angles and the
         * other six are quarter turns of them. The target recomputes the radius at each use; a
         * shared local changes the code. */
        k = (radius + (work->radius_offset >> 8)) / 4;
        for (i = 0; i < TUBE_MESH_RING_SCALE_COUNT; i++) {
            extent = (((cos0 * g_effect_tube_mesh_ring_scales[i]) >> 12) * (radius + (work->radius_offset >> 8))) >> 12;
            points[TUBE_MESH_MIDDLE_ROW + i][0].vx = points[TUBE_MESH_MIDDLE_ROW + i][2].vz
                = points[TUBE_MESH_MIDDLE_ROW - i][0].vx = points[TUBE_MESH_MIDDLE_ROW - i][2].vz = extent;
            extent = -extent;
            points[TUBE_MESH_MIDDLE_ROW + i][4].vx = points[TUBE_MESH_MIDDLE_ROW + i][6].vz
                = points[TUBE_MESH_MIDDLE_ROW - i][4].vx = points[TUBE_MESH_MIDDLE_ROW - i][6].vz = extent;
            extent = (((sin0 * g_effect_tube_mesh_ring_scales[i]) >> 12) * (radius + (work->radius_offset >> 8))) >> 12;
            points[TUBE_MESH_MIDDLE_ROW + i][0].vz = points[TUBE_MESH_MIDDLE_ROW + i][6].vx
                = points[TUBE_MESH_MIDDLE_ROW - i][0].vz = points[TUBE_MESH_MIDDLE_ROW - i][6].vx = extent;
            extent = -extent;
            points[TUBE_MESH_MIDDLE_ROW + i][2].vx = points[TUBE_MESH_MIDDLE_ROW + i][4].vz
                = points[TUBE_MESH_MIDDLE_ROW - i][2].vx = points[TUBE_MESH_MIDDLE_ROW - i][4].vz = extent;
            extent
                = (((cos45 * g_effect_tube_mesh_ring_scales[i]) >> 12) * (radius + (work->radius_offset >> 8))) >> 12;
            points[TUBE_MESH_MIDDLE_ROW + i][1].vx = points[TUBE_MESH_MIDDLE_ROW + i][3].vz
                = points[TUBE_MESH_MIDDLE_ROW - i][1].vx = points[TUBE_MESH_MIDDLE_ROW - i][3].vz = extent;
            extent = -extent;
            points[TUBE_MESH_MIDDLE_ROW + i][5].vx = points[TUBE_MESH_MIDDLE_ROW + i][7].vz
                = points[TUBE_MESH_MIDDLE_ROW - i][5].vx = points[TUBE_MESH_MIDDLE_ROW - i][7].vz = extent;
            extent
                = (((sin45 * g_effect_tube_mesh_ring_scales[i]) >> 12) * (radius + (work->radius_offset >> 8))) >> 12;
            points[TUBE_MESH_MIDDLE_ROW + i][1].vz = points[TUBE_MESH_MIDDLE_ROW + i][7].vx
                = points[TUBE_MESH_MIDDLE_ROW - i][1].vz = points[TUBE_MESH_MIDDLE_ROW - i][7].vx = extent;
            extent = -extent;
            points[TUBE_MESH_MIDDLE_ROW + i][3].vx = points[TUBE_MESH_MIDDLE_ROW + i][5].vz
                = points[TUBE_MESH_MIDDLE_ROW - i][3].vx = points[TUBE_MESH_MIDDLE_ROW - i][5].vz = extent;
            points[TUBE_MESH_MIDDLE_ROW + i][0].vy = points[TUBE_MESH_MIDDLE_ROW + i][1].vy
                = points[TUBE_MESH_MIDDLE_ROW + i][2].vy = points[TUBE_MESH_MIDDLE_ROW + i][3].vy
                = points[TUBE_MESH_MIDDLE_ROW + i][4].vy = points[TUBE_MESH_MIDDLE_ROW + i][5].vy
                = points[TUBE_MESH_MIDDLE_ROW + i][6].vy = points[TUBE_MESH_MIDDLE_ROW + i][7].vy
                = (i * k * growth.vy) >> 8;
            points[TUBE_MESH_MIDDLE_ROW - i][0].vy = points[TUBE_MESH_MIDDLE_ROW - i][1].vy
                = points[TUBE_MESH_MIDDLE_ROW - i][2].vy = points[TUBE_MESH_MIDDLE_ROW - i][3].vy
                = points[TUBE_MESH_MIDDLE_ROW - i][4].vy = points[TUBE_MESH_MIDDLE_ROW - i][5].vy
                = points[TUBE_MESH_MIDDLE_ROW - i][6].vy = points[TUBE_MESH_MIDDLE_ROW - i][7].vy
                = -((i * k * growth.vy) >> 8);
            growth.vy += growth_step.vy;
        }

        /* Grow the radius for the next frame; growth_step.vx accelerates the growth. */
        work->radius_speed += growth_step.vx;
        work->radius_offset += growth.vx + work->radius_speed;

        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        /* Keep origin components as snapshots across projection. */
        origin_x = origin.vx;
        origin_y = origin.vy;
        origin_z = origin.vz;

        /* Row 0: quad column i - 1 takes lattice rows 1 (x0/x1) and 0 (x2/x3). */
        vertex.vx = points[0][0].vx + origin_x;
        vertex.vy = points[0][0].vy + origin_y;
        vertex.vz = points[0][0].vz + origin_z;
        gte_RotTrans(&vertex, &screen2, &screen2.pad);
        quad = &work->quads[work->buffer][0];
        quad->x2 = screen2.vx;
        quad->y2 = screen2.vy;
        depth[0][0] = screen2.vz;
        vertex.vx = points[0][1].vx + origin_x;
        vertex.vy = points[0][1].vy + origin_y;
        vertex.vz = points[0][1].vz + origin_z;
        gte_RotTrans(&vertex, &screen3, &screen3.pad);
        quad->x3 = screen3.vx;
        quad->y3 = screen3.vy;
        depth[0][1] = screen3.vz;
        vertex.vx = points[1][0].vx + origin_x;
        vertex.vy = points[1][0].vy + origin_y;
        vertex.vz = points[1][0].vz + origin_z;
        gte_RotTrans(&vertex, &screen0, &screen0.pad);
        quad->x0 = screen0.vx;
        quad->y0 = screen0.vy;
        depth[1][0] = screen0.vz;
        vertex.vx = points[1][1].vx + origin_x;
        vertex.vy = points[1][1].vy + origin_y;
        vertex.vz = points[1][1].vz + origin_z;
        gte_RotTrans(&vertex, &screen1, &screen1.pad);
        quad->x1 = screen1.vx;
        quad->y1 = screen1.vy;
        depth[1][1] = screen1.vz;

        for (i = 2; i < TUBE_MESH_SEGMENT_COUNT; i++) {
            quad = &work->quads[work->buffer][i - 1];
            quad->x0 = screen1.vx;
            quad->y0 = screen1.vy;
            quad->x2 = screen3.vx;
            quad->y2 = screen3.vy;
            vertex.vx = points[0][i].vx + origin_x;
            vertex.vy = points[0][i].vy + origin_y;
            vertex.vz = points[0][i].vz + origin_z;
            gte_RotTrans(&vertex, &screen3, &screen3.pad);
            quad->x3 = screen3.vx;
            quad->y3 = screen3.vy;
            depth[0][i] = screen3.vz;
            vertex.vx = points[1][i].vx + origin_x;
            vertex.vy = points[1][i].vy + origin_y;
            vertex.vz = points[1][i].vz + origin_z;
            gte_RotTrans(&vertex, &screen1, &screen1.pad);
            quad->x1 = screen1.vx;
            quad->y1 = screen1.vy;
            depth[1][i] = screen1.vz;
        }

        /* The last quad closes the ring on quad 0. */
        quad = &work->quads[work->buffer][TUBE_MESH_LAST_SEGMENT];
        quad->x0 = screen1.vx;
        quad->y0 = screen1.vy;
        quad->x2 = screen3.vx;
        quad->y2 = screen3.vy;
        other = &work->quads[work->buffer][0];
        quad->x1 = other->x0;
        quad->y1 = other->y0;
        quad->x3 = other->x2;
        quad->y3 = other->y2;

        /* Rows 1-7: corners 2/3 repeat the previous row's corners 0/1, and lattice row j + 1
         * gives corners 0/1. The quad/other increments are dead (both are recomputed before
         * use) but lengthen the loop past the loop.c threshold at which the
         * target leaves points[n] unreduced. */
        for (j = 1; j < TUBE_MESH_RING_COUNT; j++, quad++, other++) {
            n = j + 1;

            vertex.vx = points[n][0].vx + origin_x;
            vertex.vy = points[n][0].vy + origin_y;
            vertex.vz = points[n][0].vz + origin_z;
            gte_RotTrans(&vertex, &screen0, &screen0.pad);
            quad = &work->quads[work->buffer][j * TUBE_MESH_SEGMENT_COUNT];
            quad->x0 = screen0.vx;
            quad->y0 = screen0.vy;
            depth[n][0] = screen0.vz;
            vertex.vx = points[n][1].vx + origin_x;
            vertex.vy = points[n][1].vy + origin_y;
            vertex.vz = points[n][1].vz + origin_z;
            gte_RotTrans(&vertex, &screen1, &screen1.pad);
            quad->x1 = screen1.vx;
            quad->y1 = screen1.vy;
            depth[n][1] = screen1.vz;
            other = &work->quads[work->buffer][(j - 1) * TUBE_MESH_SEGMENT_COUNT];
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            for (i = 2; i < TUBE_MESH_SEGMENT_COUNT; i++, quad++, other++, quad++) {
                quad = &work->quads[work->buffer][j * TUBE_MESH_SEGMENT_COUNT + i - 1];
                other = &work->quads[work->buffer][(j - 1) * TUBE_MESH_SEGMENT_COUNT + i - 1];
                quad->x0 = screen1.vx;
                quad->y0 = screen1.vy;
                quad->x2 = other->x0;
                quad->y2 = other->y0;
                quad->x3 = other->x1;
                quad->y3 = other->y1;
                vertex.vx = points[n][i].vx + origin_x;
                vertex.vy = points[n][i].vy + origin_y;
                vertex.vz = points[n][i].vz + origin_z;
                gte_RotTrans(&vertex, &screen1, &screen1.pad);
                quad->x1 = screen1.vx;
                quad->y1 = screen1.vy;
                depth[n][i] = screen1.vz;
            }

            /* The last quad closes the ring on the row's first quad. */
            quad = &work->quads[work->buffer][j * TUBE_MESH_SEGMENT_COUNT + TUBE_MESH_LAST_SEGMENT];
            other = &work->quads[work->buffer][(j - 1) * TUBE_MESH_SEGMENT_COUNT + TUBE_MESH_LAST_SEGMENT];
            quad->x0 = screen1.vx;
            quad->y0 = screen1.vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            other = &work->quads[work->buffer][j * TUBE_MESH_SEGMENT_COUNT];
            quad->x1 = other->x0;
            quad->y1 = other->y0;
        }

        /* Every quad maps the same texture window, scrolled by v_scroll (8.8 fixed). */
        j = work->v_scroll >> 8;
        for (i = 0; i < TUBE_MESH_QUAD_COUNT; i++) {
            quad = &work->quads[work->buffer][i];
            quad->u0 = quad->u2 = u;
            quad->v0 = quad->v1 = v + j;
            quad->u1 = quad->u3 = u + u_span;
            quad->v2 = quad->v3 = v + v_span + j;
        }

        /* Vertex colors: the color curves (neutral without them) scaled by each lattice row's brightness. */
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

        /* Lattice row r shades corners 2/3 of quad row r and corners 0/1 of quad row r - 1. */
        k = g_effect_tube_mesh_brightness_table[n][0];
        shade_r = (k * red) / ONE;
        shade_g = (k * green) / ONE;
        shade_b = (k * blue) / ONE;
        for (i = 0; i < TUBE_MESH_SEGMENT_COUNT; i++) {
            quad = &work->quads[work->buffer][i];
            setRGB2(quad, quad->r3 = shade_r, quad->g3 = shade_g, quad->b3 = shade_b);
        }

        for (j = 0; j < TUBE_MESH_RING_COUNT - 1; j++) {
            k = g_effect_tube_mesh_brightness_table[n][j + 1];
            shade_r = (k * red) / ONE;
            shade_g = (k * green) / ONE;
            shade_b = (k * blue) / ONE;
            for (i = 0; i < TUBE_MESH_SEGMENT_COUNT; i++) {
                quad = &work->quads[work->buffer][j * TUBE_MESH_SEGMENT_COUNT + i];
                other = &work->quads[work->buffer][(j + 1) * TUBE_MESH_SEGMENT_COUNT + i];
                setRGB0(quad, quad->r1 = other->r2 = other->r3 = shade_r, quad->g1 = other->g2 = other->g3 = shade_g,
                    quad->b1 = other->b2 = other->b3 = shade_b);
            }
        }

        k = g_effect_tube_mesh_brightness_table[n][TUBE_MESH_RING_COUNT];
        shade_r = (k * red) / ONE;
        shade_g = (k * green) / ONE;
        shade_b = (k * blue) / ONE;
        for (i = 0; i < TUBE_MESH_SEGMENT_COUNT; i++) {
            quad = &work->quads[work->buffer][TUBE_MESH_LAST_RING_OFFSET + i];
            setRGB0(quad, quad->r1 = shade_r, quad->g1 = shade_g, quad->b1 = shade_b);
        }

        /* Sort each quad by a quarter of its mean corner depth; quads outside the ordering table are skipped. */
        for (j = 0; j < TUBE_MESH_RING_COUNT; j++) {
            for (i = 0; i < TUBE_MESH_LAST_SEGMENT; i++) {
                n = (depth[j][i] + depth[j][i + 1] + depth[j + 1][i] + depth[j + 1][i + 1]) / 16;
                if (n >= 0 && n < EFFECT_OT_DEPTH_LIMIT) {
                    addPrim(&ot[n], &work->quads[work->buffer][j * TUBE_MESH_SEGMENT_COUNT + i]);
                }
            }

            /* The closing quad spans the last lattice column and column 0. */
            n = (depth[j][0] + depth[j][TUBE_MESH_LAST_SEGMENT] + depth[j + 1][0]
                    + depth[j + 1][TUBE_MESH_LAST_SEGMENT])
                / 16;
            if (n >= 0 && n < EFFECT_OT_DEPTH_LIMIT) {
                addPrim(&ot[n], &work->quads[work->buffer][j * TUBE_MESH_SEGMENT_COUNT + TUBE_MESH_LAST_SEGMENT]);
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
            battle_heap_free_block(work);
            record->work_slots[callback_slot] = 0;
        }
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
